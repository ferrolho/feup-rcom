#How to run on linux:
Open a terminal - **CTRL+ALT+T** - and enter the following command:
```
sh port.sh
```
The command will emulate a virtual serial port. It will seem to be frozen, but do not close it.  
Open two other terminals and browse to the bin directory of the project where you should have the executable file and a file to transfer - in this case, pinguim.gif.

On one terminal run the following command to start **receiving** the file:
```
sudo ./nserial receive /dev/ttyS0 pinguim.gif off
```

On the other terminal, run a similar command to start **transferring** the file:
```
sudo ./nserial send /dev/ttyS4 pinguim.gif off
```

Screenshots
-----------
![img](http://i.imgur.com/HfhPFNv.png)
![img](http://i.imgur.com/yCCGbe8.png)
