import java.awt.Dimension;
import java.util.Objects;
import org.videolan.StreamInfo;

public class VideoSizeTest {
    private static void check(int format, Dimension expected) {
        StreamInfo stream = new StreamInfo((byte) 0x24, (byte) format, (byte) 0,
                (char) 0, null, (byte) 0, (byte) 0);
        Dimension actual = stream.getVideoSize();
        if (!Objects.equals(expected, actual)) {
            throw new AssertionError(String.format("Format 0x%02x: expected %s, got %s",
                    format, expected, actual));
        }
    }

    public static void main(String[] args) {
        check(0x08, new Dimension(3840, 2160));
        check(0x04, new Dimension(1920, 1080));
        check(0x00, null);
        System.out.println("PASS: BD-J UHD, HD and unknown video formats");
    }
}
