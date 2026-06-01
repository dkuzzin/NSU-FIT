package exceptions;

import java.io.IOException;

public class WriterException extends IOException {
    public WriterException(String message) {
        super(message);
    }

    public WriterException(String message, Throwable cause) {
        super("[WRITE ERROR]: " + message, cause);
    }
}
