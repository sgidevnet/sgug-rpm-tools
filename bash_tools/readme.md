# SGUG Bash Helper Tools

At the end of the sugshell script there is this hook:
```
# If ~/.sgug_bashrc exists, use that as our init file.
[ -e $HOME/.sgug_bashrc ] && exec bash --rcfile $HOME/.sgug_bashrc -i
```

This directory has a dot-sgug_bashrc file for you to use and hopefully make using sgug-rse building easier.

Copy this file over to your IRIX home directory and re-name it to ".sgug_bashrc"

Example:
```
$ cp sgug_bashrc ~/.sgug_bashrc 
```

Now when starting sugshell you will see:
```
dillera@fuel ~ $ ~/rpmbuild/sgug-rse.git/sgugshell.sh 
/usr/people/dillera/rpmbuild
--------------------------------------------
setup local sg commands from ~/.sgug_bashrc
--------------------------------------------
dillera@fuel ~/rpmbuild $ 
```


## What this does for Your Workflow
This script adds an number of shell commands for use when making RPMS.

### Below are the commands with a brief description of what they do. All commands start with 'sg'

#### CD commands
1. sgbuild - moves you into the BUILD
1. sgbroot - moves you into the BUILDROOT
1. sgrpm - moves you into the rmpbuild RPMS
1. sgsrc - moves you into the rmpbuild SOURCES
1. sgspec - moves you into the rmpbuild SPEC
1. sgsrpm - moves you into the rmpbuild SRPMS
1. sgrsa - moves into sgug-rsa.git repo
1. sgtools - moves into sgug-rpm-tools.git repo


#### List Packages
1. sgmaster - lists our all available package names in sgug-rsa.git repo, master branch
1. sgwip - moves into sgug-rsa.git repo and checks out wip branch, lists wip package names

#### These commands take one argument, a package name
1. sgcp - copies the argument from sgug-rse git packages dirs into your rpmbuild tree
1. sgsbuild - moves you into the SPECS folder and installs the RPM Source package
1. sgrbuild - attempts a rpmbuild of the argument using the spce file

-end-
