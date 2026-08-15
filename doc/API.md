# mh API

mount hiding library for no-source GKI kernels. hides mountpoints from
non root readers of `/proc/pid/{mountinfo,mounts,mountstats}` by hooking
`mounts_op.show`. two registration paths: manual path list, and global
view scan that auto finds mounts whose root path contains a feature
string. offsets come from Type_info BTF at runtime. depends on LKMhook
and KallRecon.

## Init

**`int mh_init(const struct mh_cfg *cfg)`**

initializes the library: recovers kallsyms, inits the hook stack and
Type_info, resolves `mounts_op` and required symbols. `cfg` may be NULL
for defaults. returns 0 on success, -EALREADY if already inited.

```c
struct mh_cfg {
	mh_reader_fn_t reader; /* NULL: default, hide from non root readers */
	mh_mount_fn_t extra;   /* NULL: disabled */
};
```

**`void mh_exit(void)`**

restores the show hook, clears the hidden list, tears down Type_info and
the hook stack. safe to call after any failed init step.

## Proc output hiding

**`int mh_proc_enable(void)`**

patches `mounts_op.show` to the filtering dispatcher. one patch covers
mountinfo, mounts and mountstats. returns 0 on success.

**`void mh_proc_disable(void)`**

restores the original show handler. logs on restore failure.

## Hidden mountpoint list

the list is shared by proc hiding and the umount API. each entry holds
the mountpoint dentry (matching key, works across namespaces since
clone mounts share the mountpoint) and the path string.

**`int mh_hide_path(const char *path)`**

resolve `path` in the current namespace via `kern_path` and register its
mountpoint. the path must be visible to the registering process; for
per app hidden mounts use `mh_hide_scan` instead. returns 0 on success.

**`int mh_unhide_path(const char *path)`**

remove one entry by path. returns -ENOENT if absent.

**`void mh_hide_clear(void)`**

remove all entries.

**`bool mh_is_hidden(const void *mnt)`**

true if `mnt` (a `struct mount *`) matches the hidden list. exposed for
consumer verdicts.

## Global view scan

**`int mh_hide_scan(const char *feature)`**

walks every superblock via `iterate_supers`, enumerates all mount
instances on `sb->s_mounts`, renders each mount root path and registers
the mountpoint when the path contains `feature`. the walk is protected
by the kernel `mount_lock` (manually acquired qspinlock) so mounts
cannot be unmounted mid scan; collected dentries are `dget`-held until
registration completes.

no namespace dependency: finds mounts hidden from the caller's own
namespace, e.g. Magisk module mounts only visible in zygote.
requires BTF (`mh_ver`); returns -ENODATA on older kernels without it.
returns the number of registered mounts.

## Umount API

timing is fully consumer defined. call from a fork hook or anywhere
with proper credentials (e.g. `override_creds`) to actually unmount.

**`int mh_umount_path(const char *mnt, int flags)`**

unmount one path via the resolved `path_umount`. -ENOSYS when
`path_umount` is unavailable.

**`int mh_umount_all(int flags)`**

unmount every path in the hidden list. returns the last traversal
result.

## Reader rule

the default rule hides from every reader whose uid is not root. pass
`cfg->reader` to override. the callback runs inside the show path while
`namespace_sem` is held: pure CPU logic only, no sleeping, no locks.

### uid lists

consumer defined visibility per uid. the hide list wins over the allow
list, both win over the defaults.

**`int mh_reader_allow_uid(unsigned int uid)`**

the uid always sees the hidden mounts. returns -ENOSPC when the list is
full (`MH_UID_MAX`).

**`int mh_reader_hide_uid(unsigned int uid)`**

the uid never sees the hidden mounts, even when allowed by defaults or
the callback.

**`void mh_reader_reset(void)`**

clears both uid lists and the cfg callbacks.

the consumer module can take these from parameters, e.g.
`allow_uids=2000` keeps the adb shell visible while every other non
root uid stays hidden.
