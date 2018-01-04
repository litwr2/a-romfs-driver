# a-romfs-driver
It is a plain, robust and safe driver to work with romfs file images.  Just make such an image by an appropriate program (for example, _genromfs_), convert it to a linkable object by _gcc_, _as_, _objdump_, ... (it can require a special linker script for some case), link it with the driver and use it.
The driver supports normal files, directories, hard and symbolic links.
It may be especially useful for the some special embedded systems or for a new OS development.
