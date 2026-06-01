import exceptions.ReaderException;

import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.HashMap;
import java.io.FileInputStream;
import java.util.Map;

public class WordCounter {

    public static int readWords(final String filename, HashMap<String, Integer> data) throws IOException{

        try {
            int count = 0;
            Reader reader = new InputStreamReader(new FileInputStream(filename));
            StringBuilder word = new StringBuilder();

            int s;
            while ((s = reader.read()) != -1){
                boolean separator = !(Character.isLetterOrDigit((char)s));
                if (separator){
                    if (word.isEmpty()) continue;
                    data.merge(word.toString(), 1, Integer::sum);
                    word.setLength(0);
                    count++;
                }else{
                    word.append(Character.toLowerCase((char)s));
                }
            }
            if (!word.isEmpty()){
                data.merge(word.toString(), 1, Integer::sum);
                word.setLength(0);
                count++;
            }
            reader.close();
            return count;
        }
        catch (Exception e)
        {
            throw new ReaderException(e.getMessage(), e);
        }
    }
    public static ArrayList<Map.Entry<String, Integer>> sortWords(HashMap<String, Integer> data){
        ArrayList<Map.Entry<String, Integer>> sortedData= new ArrayList<>(data.entrySet());
        if (sortedData.size() <= 1) return sortedData;

        sortedData.sort((a, b) -> {
            int comp = Integer.compare(a.getValue(), b.getValue());

            return (comp != 0) ? -comp : a.getKey().compareTo(b.getKey());
        });
        return sortedData;
    }
}
