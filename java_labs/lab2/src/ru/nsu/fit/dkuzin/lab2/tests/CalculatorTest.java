package ru.nsu.fit.dkuzin.lab2.tests;

import org.junit.jupiter.api.Test;
import ru.nsu.fit.dkuzin.lab2.commands.*;
import ru.nsu.fit.dkuzin.lab2.core.*;
import ru.nsu.fit.dkuzin.lab2.exceptions.*;

import java.io.*;
import java.util.List;
import java.util.logging.*;

import static org.junit.jupiter.api.Assertions.*;
public class CalculatorTest {
    private static void turnOffLogging(){
        Logger root = Logger.getLogger("");

        for (Handler h : root.getHandlers()){
            root.removeHandler(h);
        }

    }

    @Test
    void pushValidNum() throws CalcException {
        Context ctx = new Context();
        Command push = new PushCommand();
        push.execute(ctx, List.of("5"));

        assertEquals(1, ctx.getStackSize());
        assertEquals(5.0, ctx.peek());
    }

    @Test
    void pushInvalidNum(){
        Context context = new Context();

        assertThrows(CalcException.class,
                () -> new PushCommand().execute(context, List.of("HEHeheheheHehe")));
    }

    @Test
    void pushNotDefined(){
        Context context = new Context();

        assertThrows(CalcException.class,
                () -> new PushCommand().execute(context, List.of("pipipupupa")));
    }


    @Test
    void popNoEmpty() throws CalcException{
        Context context = new Context();
        context.push(1);
        context.push(5);
        new PopCommand().execute(context, List.of());
        assertEquals(1.0, context.peek());
    }

    @Test
    void popEmptyStack(){
        Context context = new Context();
        assertThrows(CalcException.class,
                () -> new PopCommand().execute(context, List.of()));
    }

    @Test
    void defineValid() throws CalcException{
        Context context = new Context();
        new DefineCommand().execute(context, List.of("a", "5"));
        new PushCommand().execute(context, List.of("a"));
        assertEquals(5.0, context.peek());
    }

    @Test
    void defineMissArg(){
        Context context = new Context();
        assertThrows(CalcException.class,
                () -> new DefineCommand().execute(context, List.of("a")));
    }

    @Test
    void defineTooManyArg(){
        Context context = new Context();
        assertThrows(CalcException.class,
                () -> new DefineCommand().execute(context, List.of("a", "5", "a")));
    }

    @Test
    void add_WithTwoValues() throws Exception {
        Context context = new Context();
        context.push(2);
        context.push(3);

        new AddCommand().execute(context, List.of());

        assertEquals(5.0, context.peek(), 1e-9);
    }

    @Test
    void add_OneInStack() {
        Context context = new Context();
        context.push(1);

        assertThrows(CalcException.class,
                () -> new AddCommand().execute(context, List.of()));
    }
    @Test
    void add_EmptyStack() {
        Context context = new Context();

        assertThrows(CalcException.class,
                () -> new AddCommand().execute(context, List.of()));
    }

    @Test
    void sub_WithTwoValues() throws Exception {
        Context context = new Context();
        context.push(3);
        context.push(2);

        new SubCommand().execute(context, List.of());

        assertEquals(1.0, context.peek(), 1e-9);
    }

    @Test
    void sub_OneInStack() {
        Context context = new Context();
        context.push(1);

        assertThrows(CalcException.class,
                () -> new SubCommand().execute(context, List.of()));
    }

    @Test
    void sub_EmptyStack() {
        Context context = new Context();

        assertThrows(CalcException.class,
                () -> new SubCommand().execute(context, List.of()));
    }

    @Test
    void mul_WithTwoValues() throws Exception {
        Context context = new Context();
        context.push(3);
        context.push(2);

        new MulCommand().execute(context, List.of());

        assertEquals(6.0, context.peek(), 1e-9);
    }

    @Test
    void mul_OneInStack() {
        Context context = new Context();
        context.push(1);

        assertThrows(CalcException.class,
                () -> new MulCommand().execute(context, List.of()));
    }
    @Test
    void mul_EmptyStack() {
        Context context = new Context();

        assertThrows(CalcException.class,
                () -> new MulCommand().execute(context, List.of()));
    }

    @Test
    void div_OneInStack() {
        Context context = new Context();
        context.push(1);

        assertThrows(CalcException.class,
                () -> new DivCommand().execute(context, List.of()));
    }

    @Test
    void div_ValidAndTwoArgs() throws Exception {
        Context context = new Context();
        context.push(10);
        context.push(2);

        new DivCommand().execute(context, List.of());

        assertEquals(5.0, context.peek(), 1e-9);
    }

    @Test
    void div_EmptyStack() {
        Context context = new Context();

        assertThrows(CalcException.class,
                () -> new DivCommand().execute(context, List.of()));
    }

    @Test
    void div_ByZero() {
        Context context = new Context();
        context.push(10);
        context.push(0);

        assertThrows(CommandException.class,
                () -> new DivCommand().execute(context, List.of()));

    }

    @Test
    void sqrt_PositiveValue() throws Exception {
        Context context = new Context();
        context.push(9);

        new SqrtCommand().execute(context, List.of());

        assertEquals(3.0, context.peek(), 1e-9);
    }

    @Test
    void sqrt_NegativeValue() {
        Context context = new Context();
        context.push(-1);

        assertThrows(CalcException.class,
                () -> new SqrtCommand().execute(context, List.of()));
    }

    @Test
    void sqrt_Empty() {
        Context context = new Context();

        assertThrows(CalcException.class,
                () -> new SqrtCommand().execute(context, List.of()));
    }


    @Test
    void print_WithNonEmptyStack() throws CalcException {
        Context context = new Context();
        context.push(10);

        ByteArrayOutputStream out = new ByteArrayOutputStream();

        System.setOut(new PrintStream(out));
        new PrintCommand().execute(context, List.of());
        System.setOut(System.out);

        assertEquals(10.0, context.peek());
        assertEquals(1, context.getStackSize());
    }

    @Test
    void print_WithEmptyStack(){
        Context context = new Context();

        assertThrows(CalcException.class,
                () -> new PrintCommand().execute(context, List.of()));
    }

    @Test
    void script_WithComments_ShouldIgnoreThem() throws CalcException, IOException {
        turnOffLogging();

        String script = """
            # this is comment
            PUSH 9
            # another comment
            SQRT #blablablabla
            PRINT
            """;

        ByteArrayOutputStream out = new ByteArrayOutputStream();
        PrintStream oldOut = System.out;
        System.setOut(new PrintStream(out));

        try {
            new Calculator().run(new BufferedReader(new StringReader(script)));
        } finally {
            System.setOut(oldOut);
        }


        assertTrue(out.toString().contains("3"));
    }

    @Test
    void script_WithUnknownCommand_ShouldContinueExecution() throws CalcException, IOException {
        turnOffLogging();
        String script = """
            PUSH 5
            SUETA
            PUSH 7
            PRINT
            """;
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        PrintStream oldOut = System.out;
        PrintStream oldErr = System.err;
        System.setOut(new PrintStream(out));
        System.setErr(new PrintStream(out));

        try {
            new Calculator().run(new BufferedReader(new StringReader(script)));
        } finally {
            System.setOut(oldOut);
            System.setErr(oldErr);
        }

        assertTrue(out.toString().contains("7"));
    }
}
