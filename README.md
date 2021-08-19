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

We provided two solutions for running the artifacts:
- Use virtual machine (**recommended**)
- Setup from scratch

## Use virtual machine

The VM uses 32 cores and 64GB memory. The size of the image is over 70GB, and
the VM may use around 100GB disk after running artifacts. So make sure you are
using a machine with sufficient cores, memory and disk. SSD in host machine is
preferred.

### Download the VM image in Google Drive
[Google drive link](https://drive.google.com/drive/folders/1bxr4lKCqlJVEx6S57-hNylLzNHkCm03x).
Since the VM image contains llvm-9 built inside, its size is over 70GB.
If we want to download it by using command line, we need to use OAuth token.
([ref link](https://webapps.stackexchange.com/questions/126394/cant-download-large-file-from-google-drive-as-one-single-folder-always-splits))
- go to [OAuth 2.0 Playground](https://developers.google.com/oauthplayground/)
- under **Step 1** box, scroll down to **Drive API v3**
- expand it and select **https://www.googleapis.com/auth/drive.readonly**
- click on the blue **Authorize APIs button**
- login if you are prompted
- and then click on **Exchange authorization code for tokens** to get a token in
  **Step 2**
- copy the **Access token** for further use
```bash
# command to download the VM
# replace ttt wiht your Access token
curl -H "Authorization: Bearer ttt" https://www.googleapis.com/drive/v3/files/1Z-PEObKkaL6SIbE83B_OxjKRnU9mHkGl?alt=media -o witcher-sosp21-ae.img
```

### Start the VM and connect to it
Running the artifacts may cost over 10 hours, so make sure your ssh connection
will not lose and the host machine will not sleep or turn off.
Consider run **tmux** first before your connection.
```bash
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
### Play
After you log into the VM, you can get bugs reported by the paper without
running the artifacts:
- $WITCHER_HOME/bugs/sosp21-correctness-bugs.md
- $WITCHER_HOME/bugs/sosp21-performance-bugs.md

In the VM, all dependencies have been set up and the tool have been built, you
only need to run the following command:
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

Here we assume we are using Fedora 29, so we use **yum** for package management.

### Environment
Here we assume we are using bash.
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
    ```
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
                 tbb-devel libatomic autoconf libevent-devel automake psmisc
# redis memcached pyplt networkx
sudo pip3 install pymemcache redis matplotlib networkx
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

You can get bugs reported by the paper without running the above commands:
- $WITCHER_HOME/bugs/sosp21-correctness-bugs.md
- $WITCHER_HOME/bugs/sosp21-performance-bugs.md
