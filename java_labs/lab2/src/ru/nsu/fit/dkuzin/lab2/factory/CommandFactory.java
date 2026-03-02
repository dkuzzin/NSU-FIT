package ru.nsu.fit.dkuzin.lab2.factory;



import ru.nsu.fit.dkuzin.lab2.Main;
import ru.nsu.fit.dkuzin.lab2.commands.Command;
import ru.nsu.fit.dkuzin.lab2.exceptions.CalcException;
import ru.nsu.fit.dkuzin.lab2.exceptions.CommandException;
import ru.nsu.fit.dkuzin.lab2.exceptions.FactoryException;

import java.io.IOException;
import java.io.InputStream;
import java.util.HashMap;
import java.util.Map;
import java.util.Properties;
import java.util.logging.Logger;

public class CommandFactory {
    private Map<String, String> commandMap;
    private static final Logger log = Logger.getLogger(CommandFactory.class.getName());
    public CommandFactory() throws FactoryException {
        commandMap = new HashMap<>();
        loadConfig();
    }
    private void loadConfig() throws FactoryException {
        log.info("Loading commands configuration");

        try(InputStream ConfigInput = CommandFactory.class.getResourceAsStream(
                "/ru/nsu/fit/dkuzin/lab2/resources/commands.properties"))
        {
            if (ConfigInput == null){
                throw new FactoryException("commands.properties not found");
            }
            Properties properties = new Properties();
            properties.load(ConfigInput);
            for (String key : properties.stringPropertyNames()){
                String value = properties.getProperty(key);
                commandMap.put(key.toUpperCase(), value.trim());
            }
            log.info("Configuration loaded successfully");
        } catch (IOException e) {
            throw new FactoryException("Failed to load config file", e);
        }
    }

    public Command createCommand(String token) throws CommandException, FactoryException {
        log.fine("Making command " + token);
        String nameOfClass = commandMap.get(token.toUpperCase());
        if (nameOfClass == null){
            throw new CommandException("Unknown command: " + token);
        }

        try{
            return (Command) Class.forName(nameOfClass).newInstance();
        }catch (ReflectiveOperationException e){
            throw new FactoryException("Cannot to create class of command in factory", e);
        }
    }



}
