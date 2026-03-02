package ru.nsu.fit.dkuzin.lab2.core;
import java.util.*;
import ru.nsu.fit.dkuzin.lab2.exceptions.CalcException;
import ru.nsu.fit.dkuzin.lab2.exceptions.CommandException;

public class Context {
    private Deque<Double> stack;
    private Map<String, Double> variables; //Лучше интерфейсы чем реализации?
    public Context(){
        variables = new HashMap<>();
        stack = new ArrayDeque<>();
    }
    public int getStackSize(){
        return stack.size();
    }

    public void addVariable(String name, double value) throws CalcException{
        if (name == null || name.isEmpty()){
            throw new CalcException("Variable name is empty");
        }
        variables.put(name, value);
    }
    public double getVariable(String name) throws CalcException{
        if (!variables.containsKey(name)){
            throw new CalcException("Unknown variable: " + name);
        }
        return variables.get(name);
    }


    public double pop() throws CalcException {
        if (stack.isEmpty()){
            throw new CommandException("Stack is empty");
        }
        return stack.pop();
    }
    public void push(double newNum){
        stack.push(newNum);
    }

    public double peek() throws CalcException{
        if (stack.isEmpty()){
            throw new CommandException("Stack is empty");
        }
        return stack.peek();
    }

}
