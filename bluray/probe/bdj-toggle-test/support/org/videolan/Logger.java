package org.videolan;

// Only the JNI logging boundary is replaced in the standalone HAVi test.
public class Logger {
    public static Logger getLogger(String name) { return new Logger(); }
    public void info(String message) { }
    public void warning(String message) { }
    public void error(String message) { throw new AssertionError(message); }
    public static String dumpStack() { return ""; }
    public void unimplemented(String message) { throw new AssertionError(message); }
}
