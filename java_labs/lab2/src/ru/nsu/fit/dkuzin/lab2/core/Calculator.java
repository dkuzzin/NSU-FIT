package ru.nsu.fit.dkuzin.lab2.core;

import ru.nsu.fit.dkuzin.lab2.commands.Command;
import ru.nsu.fit.dkuzin.lab2.exceptions.CalcException;
import ru.nsu.fit.dkuzin.lab2.exceptions.CommandException;
import ru.nsu.fit.dkuzin.lab2.exceptions.FactoryException;
import ru.nsu.fit.dkuzin.lab2.factory.CommandFactory;

import java.io.BufferedReader;
import java.io.IOException;
import java.util.Arrays;
import java.util.List;
import java.util.logging.Level;
import java.util.logging.Logger;

public class Calculator {
    private static final Logger log = Logger.getLogger(Calculator.class.getName());
    private final Context context;
    private final CommandFactory factory;
    public Calculator() throws FactoryException {
        context = new Context();
        factory = new CommandFactory();
        log.info("The calculator was successfully initialized.");
    }
    private void processLine(String line) throws CalcException {
        int commentIndex = line.indexOf("#");
        if (commentIndex != -1){
            log.fine("The line contains comment");
            line = line.substring(0, commentIndex);
        }
        line = line.trim();
        if (line.isEmpty()){
            log.fine("DONE");
            return;
        }

        String[] splitLine = line.split("\\s+");
        String commandName = splitLine[0];
        List<String> args = Arrays.asList(splitLine).subList(1, splitLine.length);

        Command command = factory.createCommand(commandName.toUpperCase());
        log.fine("Successful to create. Executing command " + commandName.toUpperCase());
        command.execute(context, args);
        log.fine("DONE");
    }
    public void run(BufferedReader inp) throws IOException {
        log.info("Calculator started processing");

        int numOfLine = 0;
        String line;
        while ((line = inp.readLine()) != null){
            numOfLine++;
            try{
                log.fine("Processing line #" + numOfLine);
                processLine(line);
            }catch (CommandException e){
                System.err.println("Command ERROR. line #" + numOfLine + " : " + e.getMessage());
                log.log(Level.WARNING, "Command ERROR. line #" + numOfLine + " : " , e);
            } catch (FactoryException e) {
                System.err.println("Factory ERROR. line #" + numOfLine + " : " + e.getMessage());
                log.log(Level.WARNING, "Factory ERROR. line #" + numOfLine + " : " , e);
            } catch (CalcException e) {
                System.err.println("Calculate ERROR. line #" + numOfLine + " : " + e.getMessage());
                log.log(Level.WARNING, "Calculate ERROR. line #" + numOfLine + " : " , e);
            }
        }
        log.info("Calculator finished processing");
    }
}
