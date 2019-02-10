{
  ALLINCS = $$DEPENDPATH $$QMAKE_INCDIR
  INCS = $$join(ALLINCS," -I","-I",)
  #message("INCS="$${INCS})

  #message("QMAKE_CXXFLAGS="$${QMAKE_CXXFLAGS})

  INCFLAGS = $$INCS $$QMAKE_CXXFLAGS
  #INCFLAGS -= "-fdiagnostics-format=msvc"
  #message("INCFLAGS="$${INCFLAGS})
  
  BISONOUT = $${OBJECTS_DIR}/${QMAKE_FILE_BASE}
  BISONCXX = $${BISONOUT}.cxx
  BISONHXX = $${BISONOUT}.hxx
  DEPCMD = "clang++ -MM -E $$INCFLAGS $$BISONCXX | sed 's!^.*: !!'"
  message("Gathering parser dependencies")

  BISONCMD = bison -d -p ${QMAKE_FILE_BASE} -o $$BISONCXX --defines=$$BISONHXX ${QMAKE_FILE_IN}
  #message("BISONCMD="$$BISONCMD)

  bison.name = Bison ${QMAKE_FILE_IN}
  bison.input = BISONSOURCES
  bison.output = $$BISONCXX
  bison.commands = $$BISONCMD
  bison.depend_command = $$DEPCMD
  bison.CONFIG += echo_depend_creation
  bison.variable_out = GENERATED_SOURCES

  !silent:bison.commands = @echo Bison ${QMAKE_FILE_IN} && $$bison.commands
  #!silent:bison.depend_command = echo Checking parser dependencies ${QMAKE_FILE_IN} && echo $$BISONCMD && $$bison.depend_command

  QMAKE_EXTRA_COMPILERS += bison
  QMAKE_CLEAN += $$BISONCXX
  QMAKE_CLEAN += $$BISONHXX
}
