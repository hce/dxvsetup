import filecmp
import glob
import os
import subprocess
import sys
import sha

def scandirs(path, origpathlen):
    files = set()
    for currentFile in glob.glob( os.path.join(path, '*') ):
        if os.path.isdir(currentFile):
            files.update(scandirs(os.path.join(path, currentFile), origpathlen))
        elif os.path.isfile(currentFile):
            cthecfn = currentFile[origpathlen:]
            if cthecfn.endswith(".alwaysreplace") or cthecfn.endswith(".alwayscopy"):
                pass
            else:
                files.add(cthecfn)
        else:
            print "Unknown type %s" % os.path.join(path, currentFile)
            sys.exit(1)
    return files

def doscan(path):
    return scandirs(path, len(path) + 1)

def ourfcmp(fb, f1, f2, shallow):
    sys.stdout.write("    COMP %s:" % fb)
    sys.stdout.flush()
    if os.path.isfile(f2 + ".alwaysreplace"): return True
    if os.path.isfile(f2 + ".alwayscopy"): return True
    res = filecmp.cmp(f1, f2, shallow)
    if res:
        sys.stdout.write("EQUAL\n")
    else:
        sys.stdout.write("DIFF\n")
    return res

def doit(orig, mod, patchpath):
    origfiles = set([fn for fn in doscan(orig) if not os.path.isfile(os.path.join(mod, fn) + ".alwaysreplace")])
    modfiles = doscan(mod)
    commonfiles = origfiles.intersection(modfiles)
    newfiles = modfiles.difference(origfiles)
    filesnotneeded = origfiles.difference(modfiles)
    directories = set([os.path.dirname(entry) for entry in modfiles])

    try:
        os.mkdir(patchpath)
    except:
        print "Unable to create %s" % patchpath
        sys.exit(1)

    print "Checking for differences in %d common files, can take a while." % len(commonfiles)
    commonfiles = [cf for cf in commonfiles if (ourfcmp(cf, os.path.join(orig, cf), os.path.join(mod, cf), False) == False)]
    print "%d file(s) need patching." % len(commonfiles)

    f = open(os.path.join(patchpath, "modfiles.txt"), "wb")
    for fnn in modfiles:
        fnn_hash = sha.sha(open(os.path.join(mod, fnn), "rb").read()).hexdigest()
        f.write("%s %s\n" % (fnn, fnn_hash))
    f.close()

    f = open(os.path.join(patchpath, "tobepatched.txt"), "wb")
    for fnn in commonfiles:
        fnn_hash = sha.sha(open(os.path.join(mod, fnn), "rb").read()).hexdigest()
        f.write("%s %s\n" % (fnn, fnn_hash))
    f.close()

    f = open(os.path.join(patchpath, "newfiles.txt"), "wb")
    for fnn in newfiles:
        fnn_hash = sha.sha(open(os.path.join(mod, fnn), "rb").read()).hexdigest()
        f.write("%s %s\n" % (fnn, fnn_hash))
    f.close()

    f = open(os.path.join(patchpath, "directories.txt"), "wb")
    for fnn in directories:
        f.write(fnn)
        f.write("\n")
    f.close()

    print "Creating PATCHES for the other files"
    for pf in commonfiles:
        h = sha.sha(open(os.path.join(orig, pf), "rb").read()).hexdigest()
        mkpatch(os.path.join(orig, pf), os.path.join(mod, pf), os.path.join(patchpath, h + ".patch"))

    print "Copying NEW files to %s" % patchpath
    for nf in newfiles:
        nf_hash = sha.sha(open(os.path.join(mod, nf), 'rb').read()).hexdigest()
        print "%s => %s" % (nf, nf_hash)
        docopy(os.path.join(mod, nf), os.path.join(patchpath, nf_hash) + ".new")

def mkpatch(orig, modf, patchfile):
    cmd = ["bsdiff", orig, modf, patchfile]
    print "Running " + " ".join(cmd)
    subprocess.call(cmd)

def docopy(src, dst):
    sys.stdout.write("    COPY %s => %s:" % (src, dst))
    sys.stdout.flush()
    fs = open(src, "rb")
    fc = fs.read()
    fs.close()
    fd = open(dst, "wb")
    fd.write(fc)
    fd.close()
    sys.stdout.write("copied %d byte(s)\n" % len(fc))
    

if __name__ == '__main__':
    try:
        orig, mod, patchfn = sys.argv[1:]
    except:
        print "Usage: %s PATH_TO_ORIGINALGAME PATH_TO_MOD PATH_TO_PATCHDIR" % sys.argv[0]
        sys.exit(1)
    doit(orig, mod, patchfn)
