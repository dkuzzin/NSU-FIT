package ru.nsu.fit.dkuzin.lab2;

import ru.nsu.fit.dkuzin.lab2.core.Calculator;
import ru.nsu.fit.dkuzin.lab2.exceptions.FactoryException;

import java.io.*;
import java.util.logging.*;



public class Main {
    private static final String pathOfLogger = "logs/calc.log";
    private static final Logger log = Logger.getLogger(Main.class.getName());
    private static void setupLogging(){
        Logger root = Logger.getLogger("");
        root.setLevel(Level.FINE);

        for (Handler h : root.getHandlers()){
            root.removeHandler(h);
        }

        try{
            FileHandler fh = new FileHandler(pathOfLogger, true);
            fh.setLevel(Level.FINE);
            fh.setFormatter(new SimpleFormatter());

            root.addHandler(fh);
        }catch(IOException e){
            System.err.println("[WARNING] Failed to setup logging " + e.getMessage());
        }
    }

    private static void runCalc(BufferedReader reader){
        try{
            Calculator calc = new Calculator();
            calc.run(reader);
        } catch (FactoryException e) {
            System.err.println("Factory error: " + e.getMessage());
            log.log(Level.SEVERE, e.getMessage(), e);
        } catch (IOException e) {
            System.err.println("Input error: " + e.getMessage());
            log.log(Level.SEVERE, e.getMessage(), e);
        }


    }

    public static void main(String args[]) {
        setupLogging();
        log.info("===================Application started===================\n");

        if (args.length == 1){
            log.fine("Got the filename " + args[0]);

            try(BufferedReader reader = new BufferedReader(new FileReader(args[0]))){
                runCalc(reader);
            }catch (IOException e) {
                System.err.println("Cannot open file: " + e.getMessage());
                log.log(Level.SEVERE, e.getMessage(), e);
            }
        }else if (args.length == 0){
            log.fine("Didn't get the filename, using System.in");

            BufferedReader reader = new BufferedReader(new InputStreamReader(System.in));
            runCalc(reader);
        }else{
            System.err.println("The calculator need 0/1 argument (filename), " +
                    "you gave " + args.length);
            log.log(Level.WARNING, "The calculator need 0/1 argument (filename), " +
                    "you gave " + args.length);
            return;
        }
        log.info("============== Application was finished ==============\n\n\n\n");
    }
}