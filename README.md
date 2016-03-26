## What is partclone-nbd?
Partclone-nbd aims to be a smart and fast tool, which allows you to browse clonezilla images without restoring it.

Currently at beta state, because it lacks on signal support. When you hit Ctrl-C, partclone-nbd behaves like after KILL signal.

## Installing
```
 $ cmake CMakeLists.txt
 $ make
 # make install
```

## Linux
On Linux you can use partclone-nbd out of the box (the kernel module `nbd` is shipped with your Linux distribution). For server mode you'll need an external client, for example an official `nbd-client`, usually shipped in a package `nbd` (`yoe/nbd` on github).

## MacOS X
On MacOS X you'll need an externel kernel module. Currently, the only one is known: `elsteveogrande/osx-nbd`, but unfortunatelly it was abadoned few years ago. I haven't tested it yet, but it seems to be very simple. External client (needed for server mode) is shipped with this module.

## Usage

### client mode (prefered)
```
 # modprobe nbd 
 # partclone-nbd -c ~/your/image.pc
 # mount /dev/nbd0 /mount/path
```
### server mode
On server machine:
```
 $ partclone-nbd -s ~/your/image.pc
```

On client machine:
```shell
 # modprobe nbd
 # nbd-client 216.IP.ADDR.OF.SERVER.142 /dev/nbd0
 # mount /dev/nbd0 /mount/path
```

Of course client and server could be on the same machine, but in this case using client mode is a better idea.
