#How to run on linux:
Open a terminal - **CTRL+ALT+T** - and enter the following command:
```
sudo socat PTY,link=/dev/ttyS0 PTY,link=/dev/ttyS4; printf "\nClosing virtual serial port... Done.\n"
```
The command will emulate a virtual serial port. It will seem to be frozen, but do not close it.  
Open two other terminals and browse to the bin directory of the project where you should have the executable file and a file to transfer - in this case, pinguim.gif.

On one terminal run the following command to start **receiving** the file:
```
sudo ./proj1 receive /dev/ttyS0 pinguim.gif
```

On the other terminal, run a similar command to start **transferring** the file:
```
sudo ./proj1 send /dev/ttyS4 pinguim.gif
```
