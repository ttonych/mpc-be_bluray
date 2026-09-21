package org.videolan;

import org.havi.ui.HLook;

// No running disc or drawing surface is needed to test the real HAVi buttons.
public class BDJXletContext {
    public static HLook getXletDefaultLook(String key, Class type) { return null; }
    public static void setXletDefaultLook(String key, HLook look) { }
}
