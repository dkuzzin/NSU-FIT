import exceptions.WriterException;

import java.io.BufferedWriter;
import java.io.FileWriter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Map;

public class CsvWriter {

    public static void write(String filename, ArrayList<Map.Entry<String, Integer>> data, int total) throws IOException {

        try {
            BufferedWriter writer = new BufferedWriter(new FileWriter(filename));
            writer.write('\uFEFF');

            writer.write("Word;Frequency;(%)\n");
            for (Map.Entry<String, Integer> i : data){
                writer.write(i.getKey() + ";" + i.getValue() + ";" + ((double)i.getValue()/total*100) + "%" + "\n");
            }
            writer.close();
        }
        catch (Exception e)
        {
            throw new WriterException(e.getMessage(), e);
        }

    }

}
