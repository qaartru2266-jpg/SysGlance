Option Explicit

Dim fileSystem, shell, root, executable
Set fileSystem = CreateObject("Scripting.FileSystemObject")
Set shell = CreateObject("WScript.Shell")

root = fileSystem.GetParentFolderName(WScript.ScriptFullName)
executable = root & "\build\Release\SysGlance.exe"

If Not fileSystem.FileExists(executable) Then
    MsgBox "未找到 SysGlance 可执行文件：" & vbCrLf & executable & _
           vbCrLf & vbCrLf & "请先按 README.md 中的说明完成构建。", _
           vbExclamation, "SysGlance"
    WScript.Quit 1
End If

shell.Run Chr(34) & executable & Chr(34), 0, False
