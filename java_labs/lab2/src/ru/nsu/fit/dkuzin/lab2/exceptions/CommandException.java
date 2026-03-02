package ru.nsu.fit.dkuzin.lab2.exceptions;

public class CommandException extends CalcException {
    public CommandException(String message){
        super(message);
    }

    public CommandException(String message, Throwable cause){
        super(message, cause);
    }
}
