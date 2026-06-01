package ru.nsu.fit.dkuzin.lab2.commands;

import ru.nsu.fit.dkuzin.lab2.core.Context;
import ru.nsu.fit.dkuzin.lab2.exceptions.CalcException;
import ru.nsu.fit.dkuzin.lab2.exceptions.CommandException;

import java.util.List;

public class SubCommand implements Command{
    @Override
    public void execute(Context context, List<String> args) throws CalcException {
        if (!args.isEmpty()){
            throw new CommandException("Too many arguments. Need 0");
        }

        if (context.getStackSize() < 2){
            throw new CommandException("There are too few items in " +
                    "the stack to perform the sub operation.");
        }
        double b = context.pop();
        double a = context.pop();
        context.push(a - b);
    }
}
