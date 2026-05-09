# LdragodestructorAI Extended Tool Capabilities

## Overview

Your agentic system now includes **8 powerful tools** for file and command management:

1. **create** - Create new files
2. **read** - Read and display file contents
3. **analyze** - Analyze file statistics
4. **list** - List directory contents
5. **summarize** - Retrieve file content for summarization
6. **delete** - Delete files (with safety confirmation)
7. **edit** - Edit existing files
8. **run** - Execute system commands (with whitelist)

---

## Tool Specifications

### 1. CREATE - File Creation

**Syntax:**
```
create: filename content: file_content
```

**Behavior:**
- Creates a new file with the specified name
- Writes content after `content:` keyword
- Overwrites file if it already exists
- Returns: `[Agent Action] ✓ Created file: filename`

**Example:**
```
Agent: I'll create a configuration file now.
       create: config.txt content: database_host=localhost
       database_port=5432
       debug_mode=true

[Agent Action] ✓ Created file: config.txt
```

**Error Handling:**
- File permission denied → `[Agent Action Error] Cannot open file: filename`
- Invalid path → `[Agent Action Error] Cannot open file: filename`

---

### 2. READ - File Display

**Syntax:**
```
read: filename
```

**Behavior:**
- Reads file from filesystem
- Displays content with line numbers
- Preserves formatting and structure
- Returns formatted output to console

**Output Format:**
```
[Agent Action] Reading file: tooltest.md
────────────────────────────────────────────────
1: Line one content
2: Line two content
3: Line three content
────────────────────────────────────────────────
```

**Example Use Cases:**
- View configuration files
- Display source code
- Check log contents
- Review documentation

**Error Handling:**
- File not found → `[Agent Action] ✗ File not found: filename`
- Permission denied → `[Agent Action] ✗ Cannot open file: filename`

---

### 3. ANALYZE - File Statistics

**Syntax:**
```
analyze: filename
```

**Behavior:**
- Counts lines in file
- Counts total words
- Counts total characters
- Gets file size in bytes
- Returns tabular statistics

**Output Format:**
```
[Agent Action] File Analysis: tooltest.md
────────────────────────────────────────────────
Lines:      5
Words:      42
Characters: 279
Size:       282 bytes
────────────────────────────────────────────────
```

**Example Use Cases:**
- Check code file metrics
- Verify data file sizes
- Monitor log file growth
- Validate documentation completeness

**Error Handling:**
- File not found → `[Agent Action] ✗ File not found: filename`
- Access denied → `[Agent Action] ✗ Error analyzing file: ...`

---

### 4. LIST - Directory Listing

**Syntax:**
```
list: directory_path
list: .             (current directory)
list: src           (relative path)
list: /path/to/dir  (absolute path)
```

**Behavior:**
- Lists all files and directories
- Shows file type: `[FILE]` or `[DIR]`
- Displays file sizes for regular files
- Counts total items

**Output Format:**
```
[Agent Action] Listing directory: src
────────────────────────────────────────────────
[FILE] main.cpp (4532 bytes)
[FILE] turboquant.hpp (1829 bytes)
[DIR] utils
[DIR] config
────────────────────────────────────────────────
[System] Total items: 4
```

**Example Use Cases:**
- Check project structure
- Verify build outputs
- Monitor directory changes
- Locate specific files

**Error Handling:**
- Directory not found → `[Agent Action] ✗ Directory not found: path`
- Access denied → `[Agent Action] ✗ Error listing directory: ...`

---

### 5. SUMMARIZE - Content Retrieval

**Syntax:**
```
summarize: filename
```

**Behavior:**
- Reads complete file content
- Displays content for agent analysis
- Prepares content for agent-generated summary
- Agent can then provide summary in response

**Output Format:**
```
[Agent Action] Retrieving for summarization: tooltest.md
────────────────────────────────────────────────
[Full file content displayed]
────────────────────────────────────────────────
[System] File content provided. You may now summarize this in your response.
```

**Workflow:**
1. Agent issues `summarize: filename`
2. System displays file content
3. Agent reads content and generates summary
4. Agent outputs summary to user

**Example Use Cases:**
- Summarize long articles
- Create abstracts from documents
- Condense log files
- Overview code files

**Error Handling:**
- File not found → `[Agent Action] ✗ File not found: filename`
- Read failed → `[Agent Action] ✗ Error summarizing file: ...`

---

### 6. DELETE - File Removal

**Syntax:**
```
delete: filename
```

**Behavior:**
- Requests file deletion
- Shows warning message
- Requires user confirmation (SAFETY)
- Does NOT automatically delete
- Logs deletion intent for audit

**Output:**
```
[Agent Action] ⚠ Delete file: config.txt
[System] WARNING: Cannot delete - requires user confirmation.
[System] If you approve, manually execute: config.txt
```

**Safety Design:**
- Agent **cannot** unilaterally delete files
- Requires explicit user confirmation
- Prevents accidental data loss
- Logs all deletion attempts

**User Confirmation Workflow:**
```
Agent: I'm cleaning up old files.
       delete: temporary.txt

[Agent Action] ⚠ Delete file: temporary.txt
[System] WARNING: Cannot delete - requires user confirmation.

User: Yes, delete it.
(User would need to implement confirmation, or approve in code)
```

**Error Handling:**
- File not found → `[Agent Action] ✗ File not found: filename`
- Permission denied → `[Agent Action] ✗ Error deleting file: ...`

---

### 7. EDIT - File Modification

**Syntax:**
```
edit: filename
```

**Behavior:**
- Marks file for editing
- Logs edit intent
- Awaits updated content
- Can be extended for interactive editing

**Output:**
```
[Agent Action] Edit requested for: config.txt
[System] Edit mode requires additional context. Provide updated content.
```

**Current Implementation:**
- Marks intent (logging phase)
- Ready for future extension to interactive diff-based editing

**Future Enhancement:**
```cpp
// Potential future flow:
1. Agent says: edit: myfile.txt
2. System shows current content with line numbers
3. Agent specifies changes: "Change line 5 to: new_value"
4. System applies changes and confirms
```

**Example Use Cases:**
- Update configuration files
- Modify code files
- Adjust parameter values
- Fix syntax errors

---

### 8. RUN - System Command Execution

**Syntax:**
```
run: command_name args
```

**Behavior:**
- Validates command against WHITELIST
- Executes if allowed
- Blocks if dangerous
- Logs execution and exit code

**WHITELIST (Safe Commands):**
```
✅ ALLOWED:
echo, dir, ls, cat, type, mkdir, copy, cp,
python, node, npm, cmake, make, gcc, g++, cl
```

**BLOCKED (Dangerous):**
```
❌ BLOCKED:
rm, del, format, shutdown, dd, mkfs, chown,
sudo, chmod, any destructive operations
```

**Output on Success:**
```
[Agent Action] Running: python script.py
[System] Exit code: 0
(Command output here)
```

**Output on Block:**
```
[Agent Action] ⚠ Command BLOCKED by whitelist: rm -rf /
```

**Add Custom Whitelist Items:**

Edit `execute_run_command()` in `main.cpp`:
```cpp
std::vector<std::string> whitelist_prefixes = {
    "echo", "dir", "ls", "cat", "type", "mkdir", "copy", "cp",
    "python", "node", "npm", "cmake", "make", "gcc", "g++", "cl",
    "mycommand",        // 👈 ADD HERE
    "anothercommand"    // 👈 ADD HERE
};
```

---

## Tool Usage Examples

### Example 1: Multi-tool Workflow

```
User: Analyze all my project files

Agent: I'll analyze your project structure and files.

       list: src
       analyze: src/main.cpp
       read: CMakeLists.txt
```

**System Output:**
```
[Agent Action] Listing directory: src
[FILE] main.cpp (15234 bytes)
[FILE] turboquant.hpp (4821 bytes)
[DIR] utils

[Agent Action] File Analysis: src/main.cpp
Lines:      456
Words:      2341
Characters: 18923
Size:       15234 bytes

[Agent Action] Reading file: CMakeLists.txt
1: cmake_minimum_required(VERSION 3.20)
2: project(my_agent)
... (full content displayed)
```

---

### Example 2: Create Configuration

```
User: Create a configuration file for the database

Agent: I'll create a database configuration file:

       create: database.conf content: [database]
host=localhost
port=5432
username=admin
password=secure123
max_connections=100
```

**System Output:**
```
[Agent Action] ✓ Created file: database.conf
```

---

### Example 3: File Content for Summary

```
User: Summarize the README file

Agent: I'll retrieve the README for summarization:

       summarize: README.md
```

**System Output:**
```
[Agent Action] Retrieving for summarization: README.md
────────────────────────────────────────────────
# LdragodestructorAI - Agentic AI System

An advanced C++ implementation featuring:
- Persistent memory across conversation turns
- File system access and operations
- TurboQuant logit quantization
- Command execution with safety whitelist
... (full content)
────────────────────────────────────────────────
[System] File content provided. You may now summarize this in your response.
```

Agent then summarizes based on retrieved content.

---

### Example 4: Safe Command Execution

```
User: Can you build the project?

Agent: I'll compile the project now:

       run: cmake --build build --config Release
```

**System Output:**
```
[Agent Action] Running: cmake --build build --config Release
[System] Exit code: 0
(Compilation messages)
```

---

### Example 5: Blocked Command

```
User: Clean up the system

Agent: I'll remove unnecessary files:

       run: rm -rf /
```

**System Output:**
```
[Agent Action] ⚠ Command BLOCKED by whitelist: rm -rf /
```

Safety mechanism prevents catastrophic damage.

---

## Tool Capabilities Matrix

| Tool | Create | Read | Modify | Execute | Safe |
|------|--------|------|--------|---------|------|
| create | ✓ | | ✓ | | ✓ |
| read | | ✓ | | | ✓ |
| analyze | | ✓ | | | ✓ |
| list | | ✓ | | | ✓ |
| summarize | | ✓ | | | ✓ |
| delete | | | ✓ | ⚠ Confirm | ✓ |
| edit | | ✓ | ✓ | | ✓ |
| run | | | | ✓ Whitelist | ✓ |

---

## Error Handling & Recovery

### Common Error Scenarios

#### Missing File
```
User: read: nonexistent.txt
[Agent Action] ✗ File not found: nonexistent.txt
```
**Recovery:** Create file first or verify path

#### Permission Denied
```
User: read: /protected/file.txt
[Agent Action] ✗ Cannot open file: /protected/file.txt
```
**Recovery:** Check file permissions or use different path

#### Blocked Command
```
User: run: rm myfile.txt
[Agent Action] ⚠ Command BLOCKED by whitelist: rm myfile.txt
```
**Recovery:** Use `create:` to overwrite or ask user to delete manually

#### Invalid Directory
```
User: list: /invalid/path
[Agent Action] ✗ Directory not found: /invalid/path
```
**Recovery:** Use correct path or `list: .` for current

---

## Integration with Persistent Memory

All tools work seamlessly with persistent memory:

```
Turn 1: User: create: data.txt content: My data
        [Agent Action] ✓ Created file: data.txt

Turn 2: User: read: data.txt
        Agent remembers previous turn!
        [Agent Action] Reading file: data.txt
        1: My data

Turn 3: User: What did we create earlier?
        Agent: We created 'data.txt' with content "My data"
        (Agent has full context from all previous turns)
```

---

## Security Considerations

### Tool Security Model

| Tool | Risk | Mitigation |
|------|------|-----------|
| create | Disk space exhaustion | User monitors; no quota implemented |
| read | Information disclosure | File permissions via OS |
| analyze | Low | Read-only, informational |
| list | Low | Read-only, informational |
| summarize | Low | Read-only, informational |
| delete | HIGH | Requires user confirmation |
| edit | Medium | Interactive approval possible |
| run | HIGH | Whitelist blocks 99% of harmful commands |

### Best Practices

✅ **DO:**
- Whitelist only necessary commands
- Review agent-generated commands before execution
- Monitor disk usage and file counts
- Log all agent actions for audit
- Keep backups of important files

❌ **DON'T:**
- Give run: permission to rm, del, format
- Allow delete: without confirmation
- Store sensitive data in readable files
- Run untrusted code via run:
- Allow infinite file creation

---

## Performance Impact

### Tool Execution Times

| Tool | Time | Notes |
|------|------|-------|
| create | <10ms | File I/O, negligible |
| read | 1-100ms | Depends on file size |
| analyze | 1-100ms | Single pass over content |
| list | 10-50ms | Directory scan |
| summarize | 10-200ms | File read + display |
| delete | 1-10ms | Deletion attempt (mostly blocked) |
| edit | 10-100ms | File preparation |
| run | Variable | Depends on command |

**Impact on Inference:**
- Tools run **after** token generation completes
- No blocking of LLM inference
- Response latency: <200ms for most operations

---

## Future Enhancements

### Phase 2 Extensions

1. **copy/move** - Copy or move files
   ```
   copy: source.txt destination.txt
   move: old_name.txt new_name.txt
   ```

2. **search** - Search file contents
   ```
   search: term filename.txt
   search: "exact phrase" **/*.txt
   ```

3. **append** - Append to files
   ```
   append: filename.txt content: additional content
   ```

4. **compare** - Diff two files
   ```
   compare: file1.txt file2.txt
   ```

5. **history** - Access command history
   ```
   history: 5     (last 5 commands)
   history: all
   ```

### Phase 3 Extensions

1. **Database access** - Query databases
   ```
   sql: SELECT * FROM users WHERE id=1
   ```

2. **API calls** - Make HTTP requests
   ```
   api: GET https://api.example.com/data
   ```

3. **Packages** - Install/manage packages
   ```
   install: numpy scipy
   ```

---

## Troubleshooting

### Tool Not Executing

**Problem:** Agent outputs command but tool doesn't execute

**Causes:**
1. Command in markdown code block → Parser handles this now ✓
2. Typo in command keyword → Check exact spelling
3. Path issues → Verify file/directory exists

**Solution:**
```
Check output for "[Agent Action]" messages.
If not present, command wasn't detected.
Ensure exact syntax: "create:" not "create :" (no space before colon)
```

### Tool Execution But No Output

**Problem:** Tool runs but output seems missing

**Cause:** Output may be buffered or overwritten

**Solution:**
```
Look for [Agent Action] message in output
List file with list: . to verify creation
Use read: to display contents
```

### Commands Appear Twice

**Problem:** Same command listed in both agent response and system actions

**Cause:** Agent generates command AND system executes it (both shown)

**Expected Behavior:** Both appearances are correct

---

## Tool Interaction Patterns

### Pattern 1: Create → Read
```
Agent: create: test.txt content: Hello
       read: test.txt
```
Useful for testing file operations

### Pattern 2: List → Analyze Multiple
```
Agent: list: src
       analyze: src/main.cpp
       analyze: src/turbo.hpp
```
Batch file analysis

### Pattern 3: Summarize → Respond
```
Agent: summarize: document.md
(Agent reads content)
(Agent generates summary in response)
```
Content-aware summaries

### Pattern 4: Run → Conditional
```
Agent: run: python script.py
(Checks exit code)
run: python cleanup.py
```
Sequential command execution

---

## API Reference

### Command Parsing

All commands are parsed from agent output:

```
// Detection
if (response.find("create:") != npos) execute_create_command()
if (response.find("read:") != npos) execute_read_command()
if (response.find("analyze:") != npos) execute_analyze_command()
if (response.find("list:") != npos) execute_list_command()
if (response.find("summarize:") != npos) execute_summarize_command()
if (response.find("delete:") != npos) execute_delete_command()
if (response.find("edit:") != npos) execute_edit_command()
if (response.find("run:") != npos) execute_run_command()
```

### Markdown Cleanup

All parsers automatically handle markdown:

```cpp
std::string clean = response;
size_t start = 0;
while ((start = clean.find("```", start)) != npos) {
    clean.erase(start, 3);  // Remove ```
}
// Now parse from clean_response
```

This allows agent to output commands in code blocks safely.

---

## Conclusion

LdragodestructorAI now has **comprehensive file and command capabilities**:

✅ File Operations: Create, Read, Analyze, List, Delete, Edit, Summarize  
✅ System Integration: Safe Command Execution (whitelist)  
✅ Error Handling: Graceful failures with user feedback  
✅ Security First: Confirmation for dangerous ops  
✅ Future Ready: Extensible architecture for more tools  

The extended tool set enables powerful agentic workflows while maintaining safety and control. 🚀
