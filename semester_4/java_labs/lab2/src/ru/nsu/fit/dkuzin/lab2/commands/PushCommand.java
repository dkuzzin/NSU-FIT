package ru.nsu.fit.dkuzin.lab2.commands;

import ru.nsu.fit.dkuzin.lab2.core.Context;
import ru.nsu.fit.dkuzin.lab2.exceptions.CalcException;
import ru.nsu.fit.dkuzin.lab2.exceptions.CommandException;

import java.util.List;

public class PushCommand implements Command {
    @Override
    public void execute(Context context, List<String> args) throws CalcException {
        if (args.size() != 1){
            throw new CommandException("Push command wants 1 arg, you get " + args.size());
        }
        String token = args.get(0);
        double parsedValue;
        try{
            parsedValue = Double.parseDouble(token);
        }catch(NumberFormatException ignored){
            parsedValue = context.getVariable(token);
        }
        context.push(parsedValue);
    }
}
