{
  ALLINCS = $$DEPENDPATH $$QMAKE_INCDIR
  INCS = $$join(ALLINCS," -I","-I",)
  #message("INCS="$${INCS})

  #message("QMAKE_CXXFLAGS="$${QMAKE_CXXFLAGS})

  INCFLAGS = $$INCS $$QMAKE_CXXFLAGS
  INCFLAGS -= "-fdiagnostics-format=msvc"
  #message("INCFLAGS="$${INCFLAGS})
  
  FLEXOUT = $${OBJECTS_DIR}/${QMAKE_FILE_BASE}
  FLEXCXX = $${FLEXOUT}.cxx
  FLEXHXX = $${FLEXOUT}.hxx
  DEPCMD = "g++ -MM -E $$INCFLAGS $$FLEXCXX | sed 's!^.*: !!'"
  message("Gathering lexer dependencies")

  flex.name = Flex ${QMAKE_FILE_IN}
  flex.input = FLEXSOURCES
  flex.output = $$FLEXCXX
  flex.commands = flex -o $$FLEXCXX --header-file=$${FLEXHXX} ${QMAKE_FILE_IN}
  flex.depend_command = $$DEPCMD
  flex.CONFIG += echo_depend_creation
  flex.variable_out = GENERATED_SOURCES

  !silent:flex.commands = @echo Lex ${QMAKE_FILE_IN} && $$flex.commands

  QMAKE_EXTRA_COMPILERS += flex
  QMAKE_CLEAN += $$FLEXCXX
  QMAKE_CLEAN += $$FLEXHXX
}
