package ru.nsu.fit.dkuzin.lab2.commands;

import ru.nsu.fit.dkuzin.lab2.core.Context;
import ru.nsu.fit.dkuzin.lab2.exceptions.CalcException;
import ru.nsu.fit.dkuzin.lab2.exceptions.CommandException;

import java.util.List;

public class DefineCommand implements Command{
    @Override
    public void execute(Context context, List<String> args) throws CalcException {
        if (args.size() != 2){
            throw new CommandException("Define command want 2 args, you get " + args.size());
        }
        String name = args.get(0).trim();
        if (name.isEmpty()){
            throw new CommandException("Name is empty or contains only spaces");
        }
        double value;
        try{
            value = Double.parseDouble(args.get(1));
        } catch (NumberFormatException e) {
            throw new CommandException("Wrong format of num. Can't parse to double");
        }
        context.addVariable(name, value);
    }
}
