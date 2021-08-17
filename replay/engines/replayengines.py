from engines.witcher.witcherengine import WitcherEngine
from engines.witchertc.witchertcengine import WitcherTCEngine
from engines.yat.yatengine import YatEngine
from engines.pmreorder.pmreorderengine import PMReorderEngine
from misc.witcherexceptions import NotSupportedOperationException
import collections

def get_engine(engine):
    if engine in engines:
        replay_engine = engines[engine]()
    else:
        raise NotSupportedOperationException(
                "Not supported replay engine: {}"
                .format(engine))

    return replay_engine

engines = collections.OrderedDict([
             ('Witcher', WitcherEngine),
             ('WitcherTC', WitcherTCEngine),
             ('Yat', YatEngine),
             ('PMReorder', PMReorderEngine)])
