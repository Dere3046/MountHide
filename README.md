# MountHide

kernel module to hide mountpoints from non root readers on ARM64 GKI.
filters `/proc/<pid>/{mountinfo,mounts,mountstats}` output so hidden
mounts never show up in a process view. per uid allow/hide rules sit on
top: hide list wins over allow list over defaults. can also really
umount a path once the hidden list is built, timing is consumer defined.
layout comes from type_info, symbols from KallRecon, hooks go through
HooKern, all memory reads use copy_from_kernel_nofault under the hood.

## license

GPL-2.0
