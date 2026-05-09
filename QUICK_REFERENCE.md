# LdragodestructorAI Agentic System - Quick Reference

## 🚀 Quick Start

### Build
```bash
cd d:\agent-0\quantum\agentic\dagonai\build
cmake --build . --config Release --target my_agent
```

### Run
```bash
cd d:\agent-0\quantum\agentic\dagonai
.\build\Release\my_agent.exe
```

---

## 📋 Command Syntax

### 1. Create File
```
Agent says: create: myfile.txt content: This is the file content
```
✅ **Result**: Creates `myfile.txt` with specified content
⚠️ **Note**: Relative paths are resolved from the project workspace root

### 2. Read File
```
Agent says: read: myfile.txt
```
✅ **Result**: Displays file contents with line numbers
📊 **Output**: Line-numbered display of entire file

### 3. Analyze File
```
Agent says: analyze: myfile.txt
```
✅ **Result**: Shows file statistics
📊 **Output**: Lines count, words count, character count, file size

### 4. List Directory
```
Agent says: list: directory_name
Agent says: list: .          (current directory)
```
✅ **Result**: Lists all files and directories
📊 **Output**: Files/folders with sizes for regular files

### 5. Summarize File
```
Agent says: summarize: myfile.txt
```
✅ **Result**: Retrieves file content for agent summarization
📊 **Output**: Full file content displayed, agent can then summarize

### 6. Delete File
```
Agent says: delete: myfile.txt
```
✅ **Result**: Requests deletion (requires user confirmation)
🛡️ **Safety**: BLOCKS automatic deletion, requires approval

### 7. Edit File
```
Agent says: edit: myfile.txt
```
✅ **Result**: Marks file for editing
📝 **Note**: Interactive editing workflow

### 8. Run Command
```
Agent says: run: python script.py
Agent says: run: cmake --build build
Agent says: run: dir
```
✅ **Result**: Executes command if in WHITELIST
🛡️ **Blocked**: Non-whitelisted commands are rejected

---

## 🛡️ Safety Whitelist

### ✅ ALLOWED Prefixes
```
echo, dir, ls, cat, type, mkdir, copy, cp,
python, node, npm, cmake, make, gcc, g++, cl
```

### ❌ BLOCKED (Examples)
```
rm, del, format, shutdown, dd, mkfs, chown,
sudo, chmod +x, any destructive operations
```

### To Add More Commands
Edit `execute_run_command()` in `main.cpp`:
```cpp
std::vector<std::string> whitelist_prefixes = {
    "echo", "dir", "ls", "cat", "type", "mkdir", "copy", "cp",
    "python", "node", "npm", "cmake", "make", "gcc", "g++", "cl",
    // ADD HERE:
    "mycommand", "anothercommand"
};
```

---

## 💾 Persistent Memory Feature

### How It Works
- **Turn 1**: Agent context = empty (0 tokens)
- **Turn 2**: Agent context = Turn 1 input + Turn 1 output (~300 tokens)
- **Turn 3**: Agent context = All previous + new input (~400 tokens)
- **...continues until context full (2048 tokens default)**

### Example
```
You: What's your name?
Agent: I'm LdragodestructorAI

You: What did you just say?
Agent: I just told you my name - I'm LdragodestructorAI
      (No confusion, agent remembers!)
```

### Reset When
- Context exceeds 2048 tokens (configurable)
- System message: "[System] Context full. Clearing old memory..."
- Agent continues but loses all prior context

---

## 📊 TurboQuant Metrics

### What It Shows
Every response ends with:
```
[TurboQuant] Final MSE Stage 2: X | Stage 3: Y | Gain: Z%
```

### Interpretation
| Gain | Meaning |
|------|---------|
| > 60% | Excellent reasoning, confident |
| 40-60% | Normal operation |
| 20-40% | Some uncertainty |
| < 5% | **Low confidence detected** ⚠️ |

### Alert Example
```
[TurboQuant Alert] Low gain (3.2%) detected. Agent may need clarification.
```
👉 **Action**: Provide clarification or rephrase your request

---

## 🔧 Configuration Options

### Context Size
In `main.cpp`, adjust:
```cpp
c_params.n_ctx = 2048;  // Change to 4096 or 8192
```
⚠️ **Trade-off**: Larger = more memory but slower, more VRAM

### Sampling Parameters
```cpp
const int   TOP_K       = 40;    // Lower = more focused
const float TOP_P       = 0.90f; // Lower = less diverse
const float TEMPERATURE = 0.80f; // Lower = sharper outputs
```

### Gain Alert Threshold
```cpp
if (improvement < 5.0f && i > 100) {  // Change 5.0f
    std::cout << "[TurboQuant Alert] Low gain (" << improvement << "%)\n";
}
```

---

## 📝 Example Conversations

### Scenario 1: File Creation
```
You: Create a shopping list file
Agent: I'll create that for you:
       create: shopping_list.txt content: Milk, Bread, Eggs, Butter, Cheese
[Agent Action] ✓ Created file: shopping_list.txt

You: Add apples to the list
Agent: I'll edit the shopping list:
       create: shopping_list.txt content: Milk, Bread, Eggs, Butter, Cheese, Apples
[Agent Action] ✓ Created file: shopping_list.txt
```

### Scenario 2: Command Execution
```
You: Can you create a Python script that counts to 10?
Agent: Sure! I'll create the script:
       create: count.py content: for i in range(1, 11): print(i)
[Agent Action] ✓ Created file: count.py

You: Now run it
Agent: I'll execute the script:
       run: python count.py
[Agent Action] Running: python count.py
[System] Exit code: 0
1
2
3
...
10
```

### Scenario 3: Blocked Command
```
You: Delete all files on the system
Agent: I'll attempt to clean things up:
       run: rm -rf /
[Agent Action] ⚠ Command BLOCKED by whitelist: rm -rf /
[System] This command violates safety constraints.
```

---

## 🔍 Debugging Tips

### No output from agent?
- Check model file: `models/phi3.gguf` exists ✓
- Run from correct directory: `d:\agent-0\quantum\agentic\dagonai` ✓
- Check console for error messages starting with `[Error]`

### Command not executing?
- Verify command prefix is in WHITELIST
- Check spelling: `run: python script.py` not `python script.py`
- Ensure agent actually outputs the command (it might refuse)

### File not created?
- Check the workspace root shown at startup (files are created relative to that path)
- Look for error: `[Agent Action Error] Cannot open file:`
- Ensure filename is valid (no special chars like `<>*?`)

### Position mismatch error?
- This indicates persistent memory issue
- Message: "inconsistent sequence positions"
- Solution: Rebuild with latest code (`cmake --build . --config Release`)

---

## 📈 Performance Metrics

### Typical Values
| Operation | Time | Memory |
|-----------|------|--------|
| Model load | ~5s | ~3GB |
| Tokenize + Decode prompt | ~1s | Included |
| Generate 100 tokens | ~5-10s | Included |
| File creation | <10ms | Minimal |
| Command execution | Variable | Variable |

### Optimization Ideas
- Use smaller model (smaller GGUF file)
- Reduce `n_ctx` (uses less VRAM)
- Increase `TOP_K` (generates faster but less creative)

---

## 🚦 State Management

### Per-Turn State
```
n_turn = tokens needed for this turn's input
n_past = total tokens in KV cache before this turn
n_cur  = position after all tokens (input + generated)

Update: n_past = n_cur (ready for next turn)
```

### Example
```
Turn 1: n_past=0,   n_turn=184, n_cur→298,  final n_past=298
Turn 2: n_past=298, n_turn=34,  n_cur→363,  final n_past=363
Turn 3: n_past=363, n_turn=50,  n_cur→450,  final n_past=450
(When n_past + n_turn > 2048, reset all)
```

---

## 🎯 Next Steps

### To Extend Functionality

1. **Add new commands**:
   - Add parsing function for `analyze: filename`
   - Implement with `std::ifstream` to read files
   - Add to command parsing block in main loop

2. **Add tool library**:
   - Create `tools.hpp` with reusable functions
   - Implement `read_file()`, `list_directory()`, `calculate_hash()`
   - Call from agent response parsing

3. **Implement conversation logging**:
   - Save each turn to JSON log
   - Include timestamps, tokens, TurboQuant metrics
   - Useful for debugging and analysis

4. **Add user confirmation workflow**:
   - Before executing `run:` commands, ask user "[y/n]?"
   - Implement with `std::getline()` for user input
   - Reject if user declines

---

## 📚 Related Files

- **Implementation**: `src/main.cpp` (main agentic loop)
- **TurboQuant**: `src/turboquant.hpp` (logit quantization)
- **Full Guide**: `AGENTIC_SYSTEM_GUIDE.md` (comprehensive docs)
- **Model**: `models/phi3.gguf` (AI model weights)

---

## 🤝 Contribute Ideas

- Better KV cache sliding window
- Tool use patterns (structured JSON)
- Multi-agent collaboration
- Persistent memory to disk
- Web UI dashboard

---

## 📞 Support

- Check console output for `[Error]` messages
- Review `AGENTIC_SYSTEM_GUIDE.md` for details
- Test with simple commands first
- Check TurboQuant gain for quality signals

---

## 🛠️ Extended Tools Summary

LdragodestructorAI now includes **8 powerful tools**:

### File Operations
| Tool | What It Does | Syntax |
|------|-------------|--------|
| **create** | Create new files | `create: file.txt content: data` |
| **read** | Display file contents with line numbers | `read: file.txt` |
| **analyze** | Show file statistics (lines, words, size) | `analyze: file.txt` |
| **list** | List directory contents | `list: .` or `list: dirname` |
| **summarize** | Get file content for summarization | `summarize: file.txt` |
| **delete** | Request file deletion (requires confirmation) | `delete: file.txt` |
| **edit** | Prepare file for editing | `edit: file.txt` |

### System Integration
| Tool | What It Does | Syntax | Safety |
|------|-------------|--------|--------|
| **run** | Execute system commands | `run: python script.py` | ✅ Whitelist |

### Tool Examples

```
# Read and analyze a file
Agent: read: config.txt
       analyze: config.txt

# List project structure
Agent: list: src

# Create and display
Agent: create: data.csv content: name,age
       read: data.csv

# Safe command execution
Agent: run: python build.py
```

### Security Features
- ✅ Whitelist for `run:` command (blocks rm, del, format, etc.)
- ✅ User confirmation required for `delete:`
- ✅ Markdown code blocks handled automatically
- ✅ All operations logged with [Agent Action] messages

### For More Details
📖 See **EXTENDED_TOOLS_GUIDE.md** for comprehensive documentation

**Happy agentic building! 🚀**
