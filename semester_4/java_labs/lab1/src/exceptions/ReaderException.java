package exceptions;

import java.io.IOException;

public class ReaderException extends IOException {
    public ReaderException(String message, Throwable cause) {
        super("[READ ERROR]: " +message, cause);
    }
}
