#NoEnv
#SingleInstance Force
#Persistent
SetBatchLines, -1

; =========================
; TRAY MENU - RESTART OPTION
; =========================
Menu, Tray, Add, Restart Script, RestartScriptLabel
Menu, Tray, Default, Restart Script
Menu, Tray, Tip, Deskplorer
Menu, Tray, Icon, E:\Theme\Deskplorer\pmanager\icon.ico


; =========================
; START PROCESSES
; =========================


Run, E:\Theme\Deskplorer\Deskplorer.exe, , , DeskplorerPID

; wait for process Deskplorer to start
process, Wait, Deskplorer.exe, 10

SetTimer, WatchProcesses, 5000
SetTimer, NeedToRestorDeskplorer, 10000

OnExit, Cleanup
return


; =========================
; RESTART SCRIPT
; =========================
RestartScriptLabel:
    Reload
return

; =========================
; WATCHDOG
; =========================
WatchProcesses:
{
    Process, Exist, Deskplorer.exe
    if (!ErrorLevel)
    {
        Run, E:\Theme\Deskplorer\Deskplorer.exe, , , DeskplorerPID
        process, Wait, Deskplorer.exe, 10
    }
}
return



NeedToRestorDeskplorer:
{
    process, Exist, Deskplorer.exe
    if (!ErrorLevel)
    {
        Run, E:\Theme\Deskplorer\Deskplorer.exe, , , DeskplorerPID
        process, Wait, Deskplorer.exe, 10
    }
}
return

; =========================
; CLEAN EXIT
; =========================
Cleanup:
{
    if (DeskplorerPID)
        Process, Close, %DeskplorerPID%

    ExitApp, 0
    return
}
