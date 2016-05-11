## What is partclone-nbd?
Partclone-nbd aims to be smart and fast tool, which allows you to browse clonezilla images without restoring it.

Probably you can use partclone-nbd out of the box (the kernel module `nbd` is shipped with your Linux distribution). For server mode, an external client is needed. An official `nbd-client` is usually shipped in a package `nbd` (`yoe/nbd` on github).

## Installing
```
 $ cmake CMakeLists.txt
 $ make
 # make install
```

## Usage

### client mode (prefered)
```
 # modprobe nbd 
 # partclone-nbd -c ~/your/image.pc
 # mount /dev/nbd0 /mount/path
```
### server mode
On a server machine:
```
 $ partclone-nbd -s ~/your/image.pc
```

On a client machine:
```
 # modprobe nbd
 # nbd-client 216.IP.ADDR.OF.SERVER.142 /dev/nbd0
 # mount /dev/nbd0 /mount/path
```

Of course client and server could be on the same machine, but in this case using client mode is a better idea.
