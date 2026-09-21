import java.awt.event.KeyEvent;
import java.awt.event.ActionEvent;
import org.havi.ui.*;
import org.havi.ui.event.*;

public class ToggleTest {
    static void check(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }
    static class Toggle extends HToggleButton {
        int actions, overrides;
        boolean stateAtAction;
        Toggle() {
            setActionCommand("settings");
            addHActionListener(new HActionListener() {
                public void actionPerformed(ActionEvent e) {
                    check(e.getSource() == Toggle.this, "event source");
                    check("settings".equals(e.getActionCommand()), "action command");
                    actions++;
                    stateAtAction = getSwitchableState();
                }
            });
        }
        public void processHActionEvent(HActionEvent e) {
            overrides++;
            super.processHActionEvent(e);
        }
        void key(int id, int code) {
            processKeyEvent(new KeyEvent(this, id, 0, 0, code, '\n'));
        }
        void enter() { key(KeyEvent.KEY_PRESSED, KeyEvent.VK_ENTER); }
        void disableInteraction() { setInteractionState(getInteractionState() | DISABLED_STATE_BIT); }
    }
    static class Sound extends HSound {
        int plays;
        public void play() { plays++; }
    }
    static class Button extends HGraphicButton {
        int actions;
        public void processHActionEvent(HActionEvent e) { actions++; super.processHActionEvent(e); }
        void enter() { processKeyEvent(new KeyEvent(this, KeyEvent.KEY_PRESSED, 0, 0, KeyEvent.VK_ENTER, '\n')); }
    }
    static class TextButton extends HTextButton {
        int actions;
        public void processHActionEvent(HActionEvent e) { actions++; super.processHActionEvent(e); }
        void enter() { processKeyEvent(new KeyEvent(this, KeyEvent.KEY_PRESSED, 0, 0, KeyEvent.VK_ENTER, '\n')); }
    }
    public static void main(String[] args) {
        Toggle t = new Toggle();
        Sound on = new Sound(), off = new Sound();
        t.setActionSound(on); t.setUnsetActionSound(off);
        t.enter();
        check(t.getSwitchableState() && t.stateAtAction && t.actions == 1 && t.overrides == 1,
              "Enter must toggle before the listener, using the component override");
        t.key(KeyEvent.KEY_RELEASED, KeyEvent.VK_ENTER);
        t.key(KeyEvent.KEY_PRESSED, KeyEvent.VK_A);
        check(t.getSwitchableState() && t.actions == 1, "release/unrelated key must not activate");
        t.enter();
        check(!t.getSwitchableState() && !t.stateAtAction && t.actions == 2, "second Enter must unset");
        check(on.plays == 1 && off.plays == 1, "set/unset sound exactly once");
        t.processHActionEvent(new HActionEvent(t, HActionEvent.ACTION_PERFORMED, "settings"));
        check(t.getSwitchableState() && t.actions == 3, "direct HAVi action must toggle too");
        t.disableInteraction(); t.enter();
        check(t.getSwitchableState() && t.actions == 3 && on.plays == 2, "disabled toggle must ignore action");

        Toggle a = new Toggle(), b = new Toggle();
        HToggleGroup group = new HToggleGroup();
        a.setToggleGroup(group); b.setToggleGroup(group);
        a.enter(); b.enter();
        check(group.getCurrent() == b && !a.getSwitchableState() && b.getSwitchableState(), "exclusive group");
        b.enter();
        check(group.getCurrent() == null && !b.getSwitchableState(), "optional group may unset");
        group.setForcedSelection(true);
        check(group.getCurrent() == a && a.getSwitchableState(), "forced initial selection");
        a.enter();
        check(group.getCurrent() == a && a.getSwitchableState() && a.stateAtAction, "forced group stays selected");
        b.enter();
        check(group.getCurrent() == b && !a.getSwitchableState() && b.getSwitchableState(), "forced group can switch");

        Button graphic = new Button(); graphic.enter();
        TextButton text = new TextButton(); text.enter();
        check(graphic.actions == 1 && text.actions == 1, "ordinary graphic/text override exactly once");
        System.out.println("PASS: toggle before listener, virtual dispatch, direct action, release, sounds, disabled and grouped buttons.");
    }
}
