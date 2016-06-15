/^#__SHARED_SETUP__/ { 
  if (shared != "" ) { 
    system("cat " sharedsetup); 
  } else {
    print "# NOT SHARED";
  }
  next;
}

/__TOP__/ {
  sub(/__TOP__/, topdir);
}

/__PROGRAM__/ {
  sub(/__PROGRAM__/, scriptname);
}


/__PROJECT__/ {
  sub(/__PROJECT__/, project);
}

/__VERSION__/ {
  sub(/__VERSION__/, version);
}

/__SYSTEM__/ {
  sub(/__SYSTEM__/, systemtype);
}

/__EST__/ {
  sub(/__EST__/, est);
}

/__MAIN__/ {
  sub(/__MAIN__/, main);
}

/__LIB__/ {
  sub(/__LIB__/, lib);
}

/__PERL__/ {
  sub(/__PERL__/, perl);
}

/__CLASSPATH__/ {
  sub(/__CLASSPATH__/, classpath);
}

/__JAVA__/ {
  sub(/__JAVA__/, java);
}

/__JAVAC__/ {
  sub(/__JAVAC__/, javac);
}

/__JAVA_VERSION__/ {
  sub(/__JAVA_VERSION__/, java_version);
}

/__JAVA_HOME__/ {
  sub(/__JAVA_HOME__/, javahome);
}

/__LDPATH__/ {
  sub(/__LDPATH__/, ldpath);
}

/__LDVAR__/ {
  sub(/__LDVAR__/, ldvar);
}

/__LIBS__/ {
  sub(/__LIBS__/, libs);
}

/__INCLUDES__/ {
  sub(/__INCLUDES__/, includes);
}

/__DEFINES__/ {
  sub(/__DEFINES__/, defines);
}

/__LINK_COMMAND__/ {
  sub(/__LINK_COMMAND__/, linkcommand);
}

/__CXX_COMMAND__/ {
  sub(/__CXX_COMMAND__/, cxxcommand);
}

/__CC_COMMAND__/ {
  sub(/__CC_COMMAND__/, cccommand);
}


{print}
