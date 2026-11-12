// SPDX-License-Identifier: GPL-3.0-or-later
import java.awt.Component;
import java.awt.Rectangle;
import java.util.ArrayList;
import java.util.List;
import org.havi.ui.HContainer;

class ContainerRepaintTest {
    static class Menu extends HContainer {
        boolean showing;
        final List<Rectangle> requests = new ArrayList<>();

        @Override public boolean isShowing() { return showing; }

        @Override public void repaint(long delay, int x, int y, int width, int height) {
            if (requests != null) requests.add(new Rectangle(x, y, width, height));
        }

        void expect(Rectangle... rectangles) {
            if (!requests.equals(List.of(rectangles))) {
                throw new AssertionError("Unexpected repaint requests: " + requests);
            }
            requests.clear();
        }
    }

    static class Button extends Component {
        @Override public boolean isShowing() {
            return isVisible() && getParent() != null && getParent().isShowing();
        }
    }

    public static void main(String[] args) {
        Menu menu = new Menu();
        menu.setSize(300, 160);
        Button first = new Button();
        first.setBounds(20, 10, 50, 30);
        Button second = new Button();
        second.setBounds(80, 10, 60, 30);
        menu.add(first);
        menu.add(second);
        menu.showing = true;
        menu.requests.clear();

        menu.remove(first);
        menu.expect(first.getBounds());
        if (menu.getComponentCount() != 1 || menu.getComponent(0) != second) {
            throw new AssertionError("The other button should remain");
        }
        menu.remove(0);
        menu.expect(second.getBounds());

        menu.add(first);
        menu.add(second);
        menu.requests.clear();
        menu.removeAll();
        menu.expect(new Rectangle(0, 0, 300, 160));
        menu.removeAll();
        menu.expect();

        menu.showing = false;
        menu.add(first);
        menu.add(second);
        menu.requests.clear();
        menu.remove(0);
        menu.removeAll();
        menu.expect();
        System.out.println("PASS: visible child removal, full removal, empty and hidden containers");
    }
}
