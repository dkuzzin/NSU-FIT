package ru.nsu.fit.dkuzin.lab2.commands;

import ru.nsu.fit.dkuzin.lab2.core.Context;
import ru.nsu.fit.dkuzin.lab2.exceptions.CalcException;

import java.util.List;
public interface Command {
    public void execute(Context context, List<String> args) throws CalcException;
}
