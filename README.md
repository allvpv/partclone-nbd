## partclone-nbd
Partclone-nbd exports partclone/clonezilla partition image using NBD protocol;
thus, it allows mount partition image directly, without restoring it to
another device. 

Partclone-nbd requires `nbd` kernel module (enabled by default in kernel config
file and available in most Linux distributions). Server mode requires external
client.  Official `nbd-client` is available in most repositories (`yoe/nbd` on
Github).

## Compilation and installation
```
 $ cmake CMakeLists.txt
 $ make
 # make install
```

## Usage

### client mode (preferred)
```
 # modprobe nbd 
 # partclone-nbd -c ~/your/image.pc
 # mount /dev/nbd0 /mount/path
```
### server mode
Set up a server:
```
 $ partclone-nbd -s ~/your/image.pc
```

Connect client:
```
 # modprobe nbd
 # nbd-client IP.ADDR /dev/nbd0
 # mount /dev/nbd0 /mount/path
```

