#!/usr/bin/env python
import os
import sys

from methods import print_error

libname = "gds_soot"
projectdir = "project"

localEnv = Environment(tools=["default"], PLATFORM="")

customs = ["custom.py"]
customs = [os.path.abspath(path) for path in customs]

opts = Variables(customs, ARGUMENTS)
opts.Update(localEnv)

Help(opts.GenerateHelpText(localEnv))

env = localEnv.Clone()

if not (os.path.isdir("godot-cpp") and os.listdir("godot-cpp")):
    print_error("""godot-cpp is not available within this folder, as Git submodules haven't been initialized.
Run the following command to download godot-cpp:

    git submodule update --init --recursive""")
    sys.exit(1)

if not (os.path.isdir("soot") and os.listdir("soot")):
    print_error("""soot is not available within this folder.
Run:

    git submodule update --init soot""")
    sys.exit(1)



env = SConscript("godot-cpp/SConstruct", {"api_version": 4.3, "env": env, "customs": customs})
# --- C++23 for both godot-cpp and our sources ---
# soot uses std::expected, std::format, etc., which need C++23.
# godot-cpp must be compiled with the same standard.
#env.Append(CXXFLAGS=["-std=c++23"])
#env["use_llvm"] = True
os.chdir(Dir("#").abspath)   # вернуть cwd в корень

if env.get("is_msvc", False) or env.get("use_llvm", False):
    env.Append(CXXFLAGS=["/std:c++latest"])
else:
    env.Append(CXXFLAGS=["-std=c++23"])

# --- soot paths ---
soot_root = os.path.abspath("soot")
if env["platform"] == "windows":
    soot_lib = os.path.join(soot_root, "build-win", "lib")
else:
    soot_lib = os.path.join(soot_root, "build-release", "lib")

env.Append(CPPPATH=[
    "src/",
    soot_root,
    os.path.join(soot_root, "common"),
    os.path.join(soot_root, "common", "carbon"),
    os.path.join(soot_root, "common", "carbon", "vm"),
    os.path.join(soot_root, "common", "carbon", "file"),
    os.path.join(soot_root, "common", "carbon", "lib"),
    os.path.join(soot_root, "common", "carbon", "gc"),
    os.path.join(soot_root, "common", "carbon", "kernel"),
    os.path.join(soot_root, "common", "script"),
    os.path.join(soot_root, "common", "script", "archive"),
    os.path.join(soot_root, "common", "util"),
    os.path.join(soot_root, "common", "type_system"),
    os.path.join(soot_root, "common", "repl"),
    os.path.join(soot_root, "third_party", "fmt", "include"),
])

env.Append(LIBPATH=[soot_lib])

# Runtime only (Carbon + its deps). Add "libsootc", "librepl",
# "replxx", "xsockets_lib" if you need Compiler / REPL inside Godot.
env.Append(LIBS=[
    "libcarbon",
    "libscript",
    "libscriptcore",
    "libtypesys",
    "libsootutils",
    "fmt",
])

# GNU ld needs --start-group/--end-group for circular static deps.
if env["platform"] == "linux":
    env.Append(LINKFLAGS=["-Wl,--start-group"])
    env.Append(LINKFLAGS=["-Wl,--end-group"])

env.Append(CPPPATH=["src/"])
sources = Glob("src/*.cpp")

if env["target"] in ["editor", "template_debug"]:
    try:
        doc_data = env.GodotCPPDocData("src/gen/doc_data.gen.cpp", source=Glob("doc_classes/*.xml"))
        sources.append(doc_data)
    except AttributeError:
        print("Not including class reference as we're targeting a pre-4.3 baseline.")

suffix = env['suffix'].replace(".dev", "").replace(".universal", "")
lib_filename = "{}{}{}{}".format(env.subst('$SHLIBPREFIX'), libname, suffix, env.subst('$SHLIBSUFFIX'))

library = env.SharedLibrary(
    "bin/{}/{}".format(env['platform'], lib_filename),
    source=sources,
)

copy = env.Install("{}/bin/{}/".format(projectdir, env["platform"]), library)

default_args = [library, copy]
Default(*default_args)
