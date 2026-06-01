package ru.nsu.fit.dkuzin.lab2.commands;

import ru.nsu.fit.dkuzin.lab2.core.Context;
import ru.nsu.fit.dkuzin.lab2.exceptions.CalcException;
import ru.nsu.fit.dkuzin.lab2.exceptions.CommandException;

import java.util.List;

public class PrintCommand implements Command{
    @Override
    public void execute(Context context, List<String> args) throws CalcException {
        if (!args.isEmpty()){
            throw new CommandException("Too many arguments. Need 0, got " + args.size());
        }
        double val = context.peek();
        System.out.println(val);
    }
}
