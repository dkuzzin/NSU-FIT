package ru.nsu.fit.dkuzin.lab2.commands;

import ru.nsu.fit.dkuzin.lab2.core.Context;
import ru.nsu.fit.dkuzin.lab2.exceptions.CalcException;
import ru.nsu.fit.dkuzin.lab2.exceptions.CommandException;

import java.util.List;

public class SqrtCommand implements Command{
    @Override
    public void execute(Context context, List<String> args) throws CalcException {
        if (!args.isEmpty()){
            throw new CommandException("Too many arguments. Need 0");
        }
        if (context.peek() < 0.0){
            throw new CommandException("You can't take the root of a negative number.");
        }
        double value = context.pop();
        context.push(Math.sqrt(value));
    }
}
