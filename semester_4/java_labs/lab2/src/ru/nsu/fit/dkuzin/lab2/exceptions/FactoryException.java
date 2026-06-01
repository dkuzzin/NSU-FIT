package ru.nsu.fit.dkuzin.lab2.exceptions;

public class FactoryException extends CalcException{
    public FactoryException(String message){
        super(message);
    }


    public FactoryException(String message, Throwable cause){
        super(message, cause);
    }
}
