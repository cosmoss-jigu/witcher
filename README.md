# SOSP'21 Witcher Artifact

This repository contains the artifact for the SOSP'21 paper:

*Xinwei Fu, Wook-Hee Kim, Ajay Paddayuru Shreepathi, Mohannad Ismail, Sunny
Wadkar, Dongyoon Lee, and Changwoo Min “Witcher: Systematic Crash Consistency
Testing for Non-Volatile Memory Key-Value Stores”, ACM Symposium on
Operating Systems Principles (SOSP), Virtual, Oct, 2021*

The artifact was tested in a machine with following specifications:
- 64-bit Fedora 29 OS
- two 16-core Intel Xeon Gold 5218 processors (2.30 Ghz)
- 192 GB DRAM
- 512 GB NVM

## Use virtual machine

Download the VM image from the [link](TODO)
```bash
# command to download the VM
TODO

# start the VM in background
qemu-system-x86_64 \
    -hda witcher-sosp21-ae.img \
    -m 64G \
    -smp 32 \
    -machine pc,accel=kvm \
    -enable-kvm \
    -vnc :5 \
    -net nic \
    -net user,hostfwd=tcp::2222-:22 \
    -daemonize

# ssh to the VM
ssh review@localhost -p 2222

# username: review
# password: sosp21
```

You can get bugs reported by the paper without running the below commands:
- $WITCHER_HOME/bugs/sosp21-correctness-bugs.md
- $WITCHER_HOME/bugs/sosp21-performance-bugs.md

All dependencies have been set up and the tool have been built, you only need to
run the following command:
```bash
cd $WITCHER_HOME/script
./run_and_get_res_fig.sh
```

After the script finishes, you can find following results in the
*$WITCHER_HOME/script* directory:
- res.csv: a partial table from Table 5 in the paper.
- Levle_Hashing.png, FAST_FAIR.png and CCEH.png: three figures in Figure 4 in
  the paper.

Note that running all applications requires a lot of time as shown in Table 5 in
the paper, this artifact by default only selects a portion of the application.
If you are interested in running other applications, please do as following:
```bash
cd $WITCHER_HOME/script
# edit the tasks.py file to select the the application you are interested in
vim tasks.py
./run_and_get_res_fig.sh
```


## Setup from scratch

### Environment

- setup .bashrc
  ```bash
  # vim ~/.bashrc and add following:

  export LLVM9_HOME=/home/review/llvm/llvm-9.0.1.src
  export LLVM9_BIN=$LLVM9_HOME/build/bin
  export WITCHER_HOME=/home/review/witcher

  export PMEM_MMAP_HINT=600000000000
  export PMEM_IS_PMEM_FORCE=1
  export NDCTL_ENABLE=n

  ulimit -c unlimited
  ```
  ```bash
  source ~/.bashrc
  ```
- setup /etc/sysctl.conf
  ```bash
  # sudo vim /etc/sysctl.conf and add following:

  kernel.core_pattern=/tmp/%e-%p.core
  ```
  ```bash
  sudo sysctl -p
  ```

### Installation of llvm and clang 9.0.1
- install llvm and clang 9.0.1
  - get source
    ```bash
    mkdir $LLVM9_HOME
    wget https://github.com/llvm/llvm-project/releases/download/llvmorg-9.0.1/llvm-9.0.1.src.tar.xz
    wget https://github.com/llvm/llvm-project/releases/download/llvmorg-9.0.1/clang-9.0.1.src.tar.xz
    wget https://github.com/llvm/llvm-project/releases/download/llvmorg-9.0.1/compiler-rt-9.0.1.src.tar.xz
    tar xvf llvm-9.0.1.src.tar.xz && rm -f llvm-9.0.1.src.tar.xz
    tar xvf clang-9.0.1.src.tar.xz && rm -f clang-9.0.1.src.tar.xz
    tar xvf compiler-rt-9.0.1.src.tar.xz && rm -f compiler-rt-9.0.1.src.tar.xz
    mv llvm-9.0.1.src $LLVM9_HOME
    mv clang-9.0.1.src $LLVM9_HOME/tools/clang
    mv compiler-rt-9.0.1.src $LLVM9_HOME/projects/compiler-rt
    ```
  - make
    ```bash
    mkdir -p $LLVM9_HOME/build
    cd $LLVM9_HOME/build
    cmake -DLLVM_ENABLE_RTTI=true ..
    make -j16

  - update .bashrc
    ```bash
    # vim ~/.bashrc and add following:
    export CC=$LLVM9_BIN/clang
    export CXX=$LLVM9_BIN/clang++
    ```
    ```bash
    source ~/.bashrc
    ```

### Dependencies
```bash
# boost and ...
sudo yum install gcc gcc-c++ vim make cmake tmux git boost-devel python3-pip \
                 tbb-devel libatomic autoconf libevent-devel automake
# redis memcached pyplt
sudo pip3 install pymemcache redis matplotlib
```

### Play
```bash
git clone git@github.com:cosmoss-vt/witcher.git
cd $WITCHER_HOME/script

# build witcher
./build.sh

# instrument apps
python3 instrument.py

# run witcher
python3 run.py

# get result
python3 get_result.py
python3 get_figure.py
```

After the script finishes, you can find following results in the
*$WITCHER_HOME/script* directory:
- res.csv: a partial table from Table 5 in the paper.
- Levle_Hashing.png, FAST_FAIR.png and CCEH.png: three figures in Figure 4 in
  the paper.

Note that running all applications requires a lot of time as shown in Table 5 in
the paper, this artifact by default only selects a portion of the application.
If you are interested in running other applications, please do as following:
```bash
cd $WITCHER_HOME/script
# edit the tasks.py file to select the the application you are interested in
vim tasks.py
./run_and_get_res_fig.sh
```

You can get bugs reported by the paper without running the above commands:
- $WITCHER_HOME/bugs/sosp21-correctness-bugs.md
- $WITCHER_HOME/bugs/sosp21-performance-bugs.md
