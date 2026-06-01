import exceptions.WriterException;

import java.io.*;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;


public class Main {
    private final static String DEFAULT_OFILENAME = "output.csv";


    public static void main(String[] args){
        if (args.length < 1 || args.length > 2){
            System.out.println("[ERROR] Incorrect number of arguments. " +
                    "Please provide the name of one file.");
            return;
        }

        final String inputPath = args[0];
        final String outputPath;
        if (args.length == 2)  outputPath = args[1];
        else{
            outputPath = DEFAULT_OFILENAME;
            System.out.println("[WARN]: You did not specify the name of the output file, " +
                    "so the result was saved " + DEFAULT_OFILENAME);
        }


        ArrayList<Map.Entry<String, Integer>> sortedData = null;
        HashMap<String, Integer> data = new HashMap<>();

        final int total;
        try{
            total =  WordCounter.readWords(inputPath, data);
            sortedData = WordCounter.sortWords(data);
            CsvWriter.write(outputPath, sortedData, total);
        }
        catch(IOException e){
            System.out.println( e.getMessage());
            return;
        }
    }
}

