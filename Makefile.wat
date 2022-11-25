
read9660.exe: read9660.obj isopath.obj
	wlink system nt file {$<} name $*

.c.obj:
	wcc386 /fr= $<
