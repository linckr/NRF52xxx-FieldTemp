# Flash 初始化故障指示开发计划

## 需求背景
用户要求在系统初始化时，如果 Flash (W25Q64) 不可用，系统应进入故障状态并持续闪烁 LED 灯进行指示。

## 实现方案

### 1. 修改 `storage_init` 函数
- **文件**: `src/main.c`
- **修改**: 将 `storage_init` 的返回类型从 `void` 改为 `int`。
- **逻辑**: 
  - 如果 `w25q64_init()` 返回非 0，`storage_init` 返回该错误码。
  - 否则返回 0。

### 2. 在 `main` 函数中处理初始化失败
- **文件**: `src/main.c`
- **位置**: 调用 `storage_init()` 处。
- **逻辑**:
  - 接收 `storage_init()` 的返回值。
  - 如果返回值不为 0（表示失败）：
    - 打印错误日志。
    - 进入死循环 `while(1)`。
    - 在循环中每 100ms 翻转所有 LED (`LED_MASK`)，实现持续快速闪烁。

### 3. 在 `storage_write_batch` 中添加 LED 指示
- **文件**: `src/main.c`
- **位置**: `storage_write_batch` 函数开头或写入成功后。
- **逻辑**:
  - 调用 `led_start_data_blink()` 函数（该函数已在 `led.c` 中实现，效果为 DATA LED 闪烁 3 次）。
  - 确保该函数调用不会阻塞 Flash 写入操作。

## 验证计划
1. **代码审查**: 确认修改逻辑正确，无编译错误。
2. **测试**:
   - **故障模拟**: 模拟 `storage_init` 返回错误，验证系统是否死循环闪烁。
   - **写入指示**: 触发 Flash 写入（例如等待数据缓冲区满），验证 DATA LED 是否闪烁 3 次。

## 待办事项
- [ ] 修改 `src/main.c` 中的 `storage_init` 声明和定义。
- [ ] 在 `main` 函数中添加错误处理和死循环闪烁逻辑。
