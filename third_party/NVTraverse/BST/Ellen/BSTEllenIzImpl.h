/* 
 * File:   ellen_impl.h
 * Author: Maya Arbel-Raviv
 *
 * Created on June 1, 2017, 4:02 PM
 */

#ifndef ELLEN_IZ_IMPL_H
#define ELLEN_IZ_IMPL_H
#include "BSTEllenIz.h"

#include "../../gc/ssmem.h"

#include <vector>
#include <assert.h>






template <typename skey_t, typename sval_t, class RecMgr>
sval_t BSTEllenIz<skey_t, sval_t, RecMgr>::bst_find(const int tid, skey_t key/*, EpochThread epoch*/) {

    	auto guard = recmgr->getGuard(tid, true);
   
        auto l = root->left; // root is immutable
        while (utils::READ_node(tid, l->left, l)) l = (key < l->key) ? utils::READ_node(tid, l->left, l) : utils::READ_node(tid, l->right, l);
    auto ret = (l->key == key) ? l->value : NO_VALUE;
    FENCE(tid);
    return ret;
}

template <typename skey_t, typename sval_t, class RecMgr>
sval_t BSTEllenIz<skey_t, sval_t, RecMgr>::bst_insert(const int tid, const skey_t key, const sval_t value/*, EpochThread epoch*/) {

    	while (1) {
        auto guard = recmgr->getGuard(tid);
            auto p = root;
            auto pupdate = p->update;
            SOFTWARE_BARRIER;
            auto l = p->left;
            while (utils::READ_node(tid, l->left, l)) {
                p = l;
                pupdate = utils::READ_node(tid, p->update, p);
                SOFTWARE_BARRIER;
                l = (key < l->key) ? utils::READ_node(tid, l->left, l) : utils::READ_node(tid, l->right, l);
            }

        if (l->key == key) {
            auto ret = l->value;
            FENCE(tid);
            return ret;
        }
        if (GETFLAG(pupdate) != STATE_CLEAN) {
            bst_help(tid, pupdate/*, epoch*/);
        } else {
            auto new_node = create_node(tid, key, value, NULL, NULL/*, epoch*/);
            IF_OURS FLUSH_STRUCT(tid, new_node);

            auto new_sibling = create_node(tid, l->key, l->value, NULL, NULL/*, epoch*/);
            IF_OURS FLUSH_STRUCT(tid, new_sibling);

            auto new_internal = (key < l->key)
                    ? create_node(tid, l->key, NO_VALUE, new_node, new_sibling/*, epoch*/)
                    : create_node(tid, key, NO_VALUE, new_sibling, new_node/*, epoch*/);

            auto op = create_iinfo_t(tid, p, new_internal, l/*, epoch*/);
            auto result = utils::FCASV_node(tid, &p->update, pupdate, FLAG(op, STATE_IFLAG), p);
            if (result == pupdate) {
                bst_help_insert(tid, op/*, epoch*/);
                FENCE(tid);
                return NO_VALUE;
            } else {
                ssmem_free(alloc, new_node);
                ssmem_free(alloc, new_sibling);
                ssmem_free(alloc, new_internal);
                ssmem_free(allocI, op);

                bst_help(tid, result/*, epoch*/);
            }
        }
    }
}

template <typename skey_t, typename sval_t, class RecMgr>
sval_t BSTEllenIz<skey_t, sval_t, RecMgr>::bst_delete(const int tid, skey_t key/*, EpochThread epoch*/) {

	while (1) {
        auto guard = recmgr->getGuard(tid);

            node_t<skey_t, sval_t> * gp = NULL;
            info_t<skey_t, sval_t> * gpupdate = NULL;
            auto p = root;
            auto pupdate = p->update;
            SOFTWARE_BARRIER;
            auto l = p->left;
            while (utils::READ_node(tid, l->left, l)) {
                gp = p;
                p = l;
                gpupdate = pupdate;
                pupdate = utils::READ_node(tid, p->update, p);
                SOFTWARE_BARRIER;
                l = (key < l->key) ? utils::READ_node(tid, l->left, l) : utils::READ_node(tid, l->right, l);
            }

        if (l->key != key) {
            FENCE(tid);

            return NO_VALUE;
        }
        auto found_value = l->value;
        if (GETFLAG(gpupdate) != STATE_CLEAN) {
            bst_help(tid, gpupdate/*, epoch*/);
        } else if (GETFLAG(pupdate) != STATE_CLEAN) {
            bst_help(tid, pupdate/*, epoch*/);
        } else {
            auto op = create_dinfo_t(tid, gp, p, l, pupdate/*, epoch*/);
            auto result = utils::FCASV_node(tid, &gp->update, gpupdate, FLAG(op, STATE_DFLAG), gp);
            if (result == gpupdate) {
                if (bst_help_delete(tid, op/*, epoch*/)) {
                    FENCE(tid);
                   // EpochEnd(epoch);
		    return found_value;
                }
            } else {
		ssmem_free(allocI, op);

		//finalize_node_list_gen((void*)op, NULL, NULL);
                //recmgr->deallocate(tid, op);
                bst_help(tid, result/*, epoch*/);
            }
        }
    }
}

template <typename skey_t, typename sval_t, class RecMgr>
bool BSTEllenIz<skey_t, sval_t, RecMgr>::bst_cas_child(const int tid, node_t<skey_t, sval_t> * parent, node_t<skey_t, sval_t> * old, node_t<skey_t, sval_t> * nnode) {
    if (old == utils::READ_node(tid, parent->left, parent)) {
        return utils::FCAS_node(tid, &parent->left, old, nnode, parent);
    } else if (old == utils::READ_node(tid, parent->right, parent)) {
        return utils::FCAS_node(tid, &parent->right, old, nnode, parent);
    } else {
        return false;
    }
}

template <typename skey_t, typename sval_t, class RecMgr>
void BSTEllenIz<skey_t, sval_t, RecMgr>::bst_help_insert(const int tid, info_t<skey_t, sval_t> * op/*, EpochThread epoch*/) {
	//EpochDeclareUnlinkNode(epoch, (void*)op->iinfo.l, sizeof(node_t<skey_t, sval_t>));

    if (bst_cas_child(tid, op->iinfo.p, op->iinfo.l, op->iinfo.new_internal)) {
        //recmgr->retire(tid, op->iinfo.l);
	ssmem_free(alloc, op->iinfo.l);
	//EpochReclaimObject(epoch, (node_t<skey_t, sval_t> *)op->iinfo.l, NULL, NULL, finalize_node_list_gen);
    }
    //EpochDeclareUnlinkNode(epoch, (void*)op, sizeof(info_t<skey_t, sval_t>));
    auto parent = op->iinfo.p;
    if (utils::FCAS_node(tid, &parent->update, FLAG(op, STATE_IFLAG), FLAG(op, STATE_CLEAN), parent)) {
        //EpochReclaimObject(epoch, (info_t<skey_t, sval_t> *)op, NULL, NULL, finalize_node_list_gen);
	//recmgr->retire(tid, op);
	ssmem_free(allocI, op);

    }
}
 
template <typename skey_t, typename sval_t, class RecMgr>
bool BSTEllenIz<skey_t, sval_t, RecMgr>::bst_help_delete(const int tid, info_t<skey_t, sval_t> * op/*, EpochThread epoch*/) {
    auto parent = op->dinfo.p;
    auto result = utils::FCASV_node(tid, &parent->update, op->dinfo.pupdate, FLAG(op, STATE_MARK), parent);
    if ((result == op->dinfo.pupdate) || (result == FLAG(op, STATE_MARK))) {
        bst_help_marked(tid, op/*, epoch*/);
        return true;
    } else {
        bst_help(tid, result/*, epoch*/);
        auto grandparent = op->dinfo.gp;
    	//EpochDeclareUnlinkNode(epoch, (void*)op, sizeof(info_t<skey_t, sval_t>));

        if (utils::FCAS_node(tid, &grandparent->update, FLAG(op, STATE_DFLAG), FLAG(op, STATE_CLEAN), grandparent)) {
            //recmgr->retire(tid, op);
	    //EpochReclaimObject(epoch, (info_t<skey_t, sval_t> *)op, NULL, NULL, finalize_node_list_gen);
            ssmem_free(allocI, op);

        }
        return false;
    }
}

template <typename skey_t, typename sval_t, class RecMgr>
void BSTEllenIz<skey_t, sval_t, RecMgr>::bst_help_marked(const int tid, info_t<skey_t, sval_t> * op/*, EpochThread epoch*/) {
    node_t<skey_t, sval_t> * other;
    auto parent = op->dinfo.p;
    if (utils::READ_node(tid, parent->right, parent) == op->dinfo.l) {
        other = utils::READ_node(tid, parent->left, parent);
    } else {
        other = utils::READ_node(tid, parent->right, parent);
    }
    //EpochDeclareUnlinkNode(epoch, (void*)op->dinfo.l, sizeof(node_t<skey_t, sval_t>));
    //EpochDeclareUnlinkNode(epoch, (void*)parent, sizeof(node_t<skey_t, sval_t>));

    if (bst_cas_child(tid, op->dinfo.gp, parent, other)) {
       //EpochReclaimObject(epoch, (node_t<skey_t, sval_t> *)op->dinfo.l, NULL, NULL/*, finalize_node_list_gen*/);
        //EpochReclaimObject(epoch, (node_t<skey_t, sval_t> *)parent, NULL, NULL/*, finalize_node_list_gen*/);
	    //recmgr->retire(tid, op->dinfo.l);
        //recmgr->retire(tid, parent
	ssmem_free(alloc, op->dinfo.l);
	ssmem_free(alloc, parent);
    }
    //EpochDeclareUnlinkNode(epoch, (void*)op, sizeof(info_t<skey_t, sval_t>));
    auto grandparent = op->dinfo.gp;
    if (utils::FCAS_node(tid, &grandparent->update, FLAG(op, STATE_DFLAG), FLAG(op, STATE_CLEAN), grandparent)) {
        ssmem_free(allocI, op);
	//EpochReclaimObject(epoch, (info_t<skey_t, sval_t> *)op, NULL, NULL, finalize_node_list_gen);
	//recmgr->retire(tid, op);
    }
}

template <typename skey_t, typename sval_t, class RecMgr>
void BSTEllenIz<skey_t, sval_t, RecMgr>::bst_help(const int tid, info_t<skey_t, sval_t> * u/*, EpochThread epoch*/) {
    if (GETFLAG(u) == STATE_DFLAG) {
        bst_help_delete(tid, UNFLAG(u)/*, epoch*/);
    } else if (GETFLAG(u) == STATE_IFLAG) {
        bst_help_insert(tid, UNFLAG(u)/*, epoch*/);
    } else if (GETFLAG(u) == STATE_MARK) {
        bst_help_marked(tid, UNFLAG(u)/*, epoch*/);
    }
}

#endif /* ELLEN_IMPL_H */


