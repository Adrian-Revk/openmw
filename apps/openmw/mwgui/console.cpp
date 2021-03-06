#include "console.hpp"

#include <components/compiler/exception.hpp>
#include <components/compiler/extensions0.hpp>

#include "../mwscript/extensions.hpp"

#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"

namespace MWGui
{
    class ConsoleInterpreterContext : public MWScript::InterpreterContext
    {
            Console& mConsole;

        public:

            ConsoleInterpreterContext (Console& console, MWWorld::Ptr reference);

            virtual void report (const std::string& message);
    };

    ConsoleInterpreterContext::ConsoleInterpreterContext (Console& console,
        MWWorld::Ptr reference)
    : MWScript::InterpreterContext (
        reference.isEmpty() ? 0 : &reference.getRefData().getLocals(), reference),
      mConsole (console)
    {}

    void ConsoleInterpreterContext::report (const std::string& message)
    {
        mConsole.printOK (message);
    }

    bool Console::compile (const std::string& cmd, Compiler::Output& output)
    {
        try
        {
            ErrorHandler::reset();

            std::istringstream input (cmd + '\n');

            Compiler::Scanner scanner (*this, input, mCompilerContext.getExtensions());

            Compiler::LineParser parser (*this, mCompilerContext, output.getLocals(),
                output.getLiterals(), output.getCode(), true);

            scanner.scan (parser);

            return isGood();
        }
        catch (const Compiler::SourceException&)
        {
            // error has already been reported via error handler
        }
        catch (const std::exception& error)
        {
            printError (std::string ("An exception has been thrown: ") + error.what());
        }

        return false;
    }

    void Console::report (const std::string& message, const Compiler::TokenLoc& loc, Type type)
    {
        std::ostringstream error;
        error << "column " << loc.mColumn << " (" << loc.mLiteral << "):";

        printError (error.str());
        printError ((type==ErrorMessage ? "error: " : "warning: ") + message);
    }

    void Console::report (const std::string& message, Type type)
    {
        printError ((type==ErrorMessage ? "error: " : "warning: ") + message);
    }

    void Console::listNames()
    {
        if (mNames.empty())
        {
            // keywords
            std::istringstream input ("");

            Compiler::Scanner scanner (*this, input, mCompilerContext.getExtensions());

            scanner.listKeywords (mNames);

            // identifier
            const MWWorld::ESMStore& store =
                MWBase::Environment::get().getWorld()->getStore();

            for (MWWorld::ESMStore::iterator it = store.begin(); it != store.end(); ++it)
            {
                it->second->listIdentifier (mNames);
            }

            // sort
            std::sort (mNames.begin(), mNames.end());
        }
    }

    Console::Console(int w, int h, bool consoleOnlyScripts)
      : WindowBase("openmw_console.layout"),
        mCompilerContext (MWScript::CompilerContext::Type_Console),
        mConsoleOnlyScripts (consoleOnlyScripts)
    {
        setCoord(10,10, w-10, h/2);

        getWidget(mCommandLine, "edit_Command");
        getWidget(mHistory, "list_History");

        // Set up the command line box
        mCommandLine->eventEditSelectAccept +=
            newDelegate(this, &Console::acceptCommand);
        mCommandLine->eventKeyButtonPressed +=
            newDelegate(this, &Console::keyPress);

        // Set up the log window
        mHistory->setOverflowToTheLeft(true);
        mHistory->setEditStatic(true);
        mHistory->setVisibleVScroll(true);

        // compiler
        Compiler::registerExtensions (mExtensions, mConsoleOnlyScripts);
        mCompilerContext.setExtensions (&mExtensions);
    }

    void Console::open()
    {
        // Give keyboard focus to the combo box whenever the console is
        // turned on
        MWBase::Environment::get().getWindowManager()->setKeyFocusWidget(mCommandLine);
    }

    void Console::close()
    {
        // Apparently, hidden widgets can retain key focus
        MWBase::Environment::get().getWindowManager()->setKeyFocusWidget(NULL);
    }

    void Console::setFont(const std::string &fntName)
    {
        mHistory->setFontName(fntName);
        mCommandLine->setFontName(fntName);
    }

    void Console::clearHistory()
    {
        mHistory->setCaption("");
    }

    void Console::print(const std::string &msg)
    {
        mHistory->addText(msg);
    }

    void Console::printOK(const std::string &msg)
    {
        print("#FF00FF" + msg + "\n");
    }

    void Console::printError(const std::string &msg)
    {
        print("#FF2222" + msg + "\n");
    }

    void Console::execute (const std::string& command)
    {
        // Log the command
        print("#FFFFFF> " + command + "\n");

        Compiler::Locals locals;
        Compiler::Output output (locals);

        if (compile (command + "\n", output))
        {
            try
            {
                ConsoleInterpreterContext interpreterContext (*this, mPtr);
                Interpreter::Interpreter interpreter;
                MWScript::installOpcodes (interpreter, mConsoleOnlyScripts);
                std::vector<Interpreter::Type_Code> code;
                output.getCode (code);
                interpreter.run (&code[0], code.size(), interpreterContext);
            }
            catch (const std::exception& error)
            {
                printError (std::string ("An exception has been thrown: ") + error.what());
            }
        }
    }

    void Console::executeFile (const std::string& path)
    {
        std::ifstream stream (path.c_str());

        if (!stream.is_open())
            printError ("failed to open file: " + path);
        else
        {
            std::string line;

            while (std::getline (stream, line))
                execute (line);
        }
    }

    void Console::keyPress(MyGUI::Widget* _sender,
                  MyGUI::KeyCode key,
                  MyGUI::Char _char)
    {
        if( key == MyGUI::KeyCode::Tab)
        {
            std::vector<std::string> matches;
            listNames();
            mCommandLine->setCaption(complete( mCommandLine->getCaption(), matches ));
#if 0
            int i = 0;
            for(std::vector<std::string>::iterator it=matches.begin(); it < matches.end(); ++it,++i )
            {
                printOK( *it );
                if( i == 50 )
                    break;
            }
#endif
        }

        if(mCommandHistory.empty()) return;

        // Traverse history with up and down arrows
        if(key == MyGUI::KeyCode::ArrowUp)
        {
            // If the user was editing a string, store it for later
            if(mCurrent == mCommandHistory.end())
                mEditString = mCommandLine->getCaption();

            if(mCurrent != mCommandHistory.begin())
            {
                --mCurrent;
                mCommandLine->setCaption(*mCurrent);
            }
        }
        else if(key == MyGUI::KeyCode::ArrowDown)
        {
            if(mCurrent != mCommandHistory.end())
            {
                --mCurrent;

                if(mCurrent != mCommandHistory.end())
                    mCommandLine->setCaption(*mCurrent);
                else
                    // Restore the edit string
                    mCommandLine->setCaption(mEditString);
            }
        }
    }

    void Console::acceptCommand(MyGUI::EditBox* _sender)
    {
        const std::string &cm = mCommandLine->getCaption();
        if(cm.empty()) return;

        // Add the command to the history, and set the current pointer to
        // the end of the list
        mCommandHistory.push_back(cm);
        mCurrent = mCommandHistory.end();
        mEditString.clear();

        execute (cm);

        mCommandLine->setCaption("");
    }

    std::string Console::complete( std::string input, std::vector<std::string> &matches )
    {
        std::string output = input;
        std::string tmp = input;
        bool has_front_quote = false;

        /* Does the input string contain things that don't have to be completed? If yes erase them. */
        /* Are there quotation marks? */
        if( tmp.find('"') != std::string::npos ) {
            int numquotes=0;
            for(std::string::iterator it=tmp.begin(); it < tmp.end(); ++it) {
                if( *it == '"' )
                    numquotes++;
            }

            /* Is it terminated?*/
            if( numquotes % 2 ) {
                tmp.erase( 0, tmp.rfind('"')+1 );
                has_front_quote = true;
            }
            else {
                size_t pos;
                if( ( ((pos = tmp.rfind(' ')) != std::string::npos ) ) && ( pos > tmp.rfind('"') ) ) {
                    tmp.erase( 0, tmp.rfind(' ')+1);
                }
                else {
                    tmp.clear();
                }
                has_front_quote = false;
            }
        }
        /* No quotation marks. Are there spaces?*/
        else {
            size_t rpos;
            if( (rpos=tmp.rfind(' ')) != std::string::npos ) {
                if( rpos == 0 ) {
                    tmp.clear();
                }
                else {
                    tmp.erase(0, rpos+1);
                }
            }
        }
        /* Erase the input from the output string so we can easily append the completed form later. */
        output.erase(output.end()-tmp.length(), output.end());

        /* Is there still something in the input string? If not just display all commands and return the unchanged input. */
        if( tmp.length() == 0 ) {
                matches=mNames;
            return input;
        }

        /* Iterate through the vector. */
        for(std::vector<std::string>::iterator it=mNames.begin(); it < mNames.end();++it) {
            bool string_different=false;

            /* Is the string shorter than the input string? If yes skip it. */
            if( (*it).length() < tmp.length() )
                continue;

            /* Is the beginning of the string different from the input string? If yes skip it. */
            for( std::string::iterator iter=tmp.begin(), iter2=(*it).begin(); iter < tmp.end();++iter, ++iter2) {
                if( tolower(*iter) != tolower(*iter2) ) {
                    string_different=true;
                    break;
                }
            }

            if( string_different )
                continue;

            /* The beginning of the string matches the input string, save it for the next test. */
            matches.push_back(*it);
        }

        /* There are no matches. Return the unchanged input. */
        if( matches.empty() )
        {
            return input;
        }

        /* Only one match. We're done. */
        if( matches.size() == 1 ) {
            /* Adding quotation marks when the input string started with a quotation mark or has spaces in it*/
            if( ( matches.front().find(' ') != std::string::npos )  ) {
                if( !has_front_quote )
                    output.append(std::string("\""));
                return output.append(matches.front() + std::string("\" "));
            }
            else if( has_front_quote ) {
                return  output.append(matches.front() + std::string("\" "));
            }
            else {
                return output.append(matches.front() + std::string(" "));
            }
        }

        /* Check if all matching strings match further than input. If yes complete to this match. */
        int i = tmp.length();

        for(std::string::iterator iter=matches.front().begin()+tmp.length(); iter < matches.front().end(); ++iter, ++i) {
            for(std::vector<std::string>::iterator it=matches.begin(); it < matches.end();++it) {
                if( tolower((*it)[i]) != tolower(*iter) ) {
                    /* Append the longest match to the end of the output string*/
                    output.append(matches.front().substr( 0, i));
                    return output;
                }
            }
        }

        /* All keywords match with the shortest. Append it to the output string and return it. */
        return output.append(matches.front());
    }

    void Console::onResChange(int width, int height)
    {
        setCoord(10,10, width-10, height/2);
    }

    void Console::setSelectedObject(const MWWorld::Ptr& object)
    {
        if (!object.isEmpty())
        {
            if (object == mPtr)
            {
                setTitle("#{sConsoleTitle}");
                mPtr=MWWorld::Ptr();
            }
            else
            {
                setTitle("#{sConsoleTitle} (" + object.getCellRef().mRefID + ")");
                mPtr = object;
            }
        }
        else
        {
            setTitle("#{sConsoleTitle}");
            mPtr = MWWorld::Ptr();
        }
        MWBase::Environment::get().getWindowManager()->setKeyFocusWidget(mCommandLine);
    }

    void Console::onReferenceUnavailable()
    {
        setSelectedObject(MWWorld::Ptr());
    }
}
