# The Chimera core's module: the OS, the disk, the window and the calls
# Chimera makes (see the README in this folder). Built only into the core.


def can_build(env, platform):
    return platform == "linuxbsd" and env.get("chimera_kit", "") != ""


def configure(env):
    pass


def get_opts(platform):
    from SCons.Variables import PathVariable

    return [
        PathVariable(
            "chimera_kit",
            "Folders holding the guest kit's headers (emulibc.h, waterbox_settings.h, jsmn.h), separated by ':'",
            "",
            PathVariable.PathAccept,
        ),
    ]
