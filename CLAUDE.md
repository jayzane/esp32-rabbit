# graphify
- **graphify** (`~/.claude/skills/graphify/SKILL.md`) - any input to knowledge graph. Trigger: `/graphify`
When the user types `/graphify`, invoke the Skill tool with `skill: "graphify"` before doing anything else.

## graphify project rules

This project has a graphify knowledge graph at graphify-out/.

Rules:
- Before answering architecture or codebase questions, read graphify-out/GRAPH_REPORT.md for god nodes and community structure
- If graphify-out/wiki/index.md exists, navigate it instead of reading raw files
- After modifying code files in this session, run `graphify update .` to keep the graph current (AST-only, no API cost)

# Superpowers
## writing-plans
- 使用/superpowers:writing-plans时候要补充pytest编写关于接口和通过gstack的/qa这个skill进行前端UI交互的集成测试

## TDD原则
- Implementation plans 写了 TDD 就要执行，不能因为"代码简单"或"mechanical task"跳过测试步骤
- 先写失败测试，再写实现——bug 在测试失败时暴露，不是靠 review 推断
- 简单代码更容易藏错，TDD 反而更重要