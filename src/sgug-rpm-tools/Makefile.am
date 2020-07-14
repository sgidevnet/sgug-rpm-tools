NULL=

bin_PROGRAMS=sgug_world_builder \
	sgug_minimal_computer

sgug_world_builder_SOURCES=				\
	dependencyset.hpp				\
	helpers.hpp					\
	installedrpm.hpp				\
	sgug_dep_engine.hpp				\
	specfile.hpp					\
	standalonerpm.hpp				\
	helpers.cpp					\
	installedrpm.cpp				\
	sgug_world_builder.cpp				\
	specfile.cpp					\
	standalonerpm.cpp				\
	$(NULL)

sgug_minimal_computer_SOURCES=				\
	dependencyset.hpp				\
	helpers.hpp					\
	installedrpm.hpp				\
	sgug_dep_engine.hpp				\
	specfile.hpp					\
	standalonerpm.hpp				\
	helpers.cpp					\
	installedrpm.cpp				\
	sgug_dep_engine.cpp				\
	sgug_minimal_computer.cpp			\
	specfile.cpp					\
	standalonerpm.cpp				\
	$(NULL)

AM_CFLAGS=						\
	$(DICL_DEPS_CFLAGS)				\
	$(RPMTOOLS_DEPS_CFLAGS)				\
	$(NULL)

sgug_world_builder_LDADD=				\
	$(DICL_DEPS_LIBS)				\
	$(RPMTOOLS_DEPS_LIBS)				\
	-lrpmbuild					\
	$(NULL)

sgug_minimal_computer_LDADD=				\
	$(DICL_DEPS_LIBS)				\
	$(RPMTOOLS_DEPS_LIBS)				\
	-lrpmbuild					\
	$(NULL)

CLEANFILES=						\
	.libs						\
	$(NULL)
