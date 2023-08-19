# Building debian packages

There is a directory for each subproject, with a directory with the packaging files for each debian based distro.
Those also contain a makefile. If you just type `make` in there, it'll build it all, but there are some more targets:

| command        | description |
|----------------|-------------|
| `make archive` | Uses git archive to create the orig.tar.xz. The commit used is specified in the makefile. |
| `make package` | Create the debian source package (debian.tar.xz and .dsc). The version will be taken from the debian/changelog file, the name from the debian/control file. |
| `make debpkg`  | Same as make all. Builds the package using debuild. |
| `make clean`   | Removes everything. |
