#include "localization.h"

#include <stddef.h>

static const wchar_t *const english[UI_COUNT] = {
    L"AudioCommander v9.3", L"AudioCommander Settings", L"AudioCommander v9.3 Help",
    L"AudioCommander", L"Unknown Windows error.", L"%ls failed.\n\n%ls",
    L"Select an audio file to view its information.",
    L"The selected path is too long to display.",
    L"Folder\r\nName: %ls\r\nPath: %ls", L"N/A for compressed codec",
    L"Name: %ls\r\nPath: %ls\r\nExtension: %ls\r\nContainer: %ls\r\nCodec: %ls\r\n"
    L"Size: %llu bytes (%llu KB)\r\nDuration: %lu:%02lu.%03lu\r\n"
    L"Bitrate: %lu kbps\r\nBitrate mode: %ls\r\nSample rate: %lu Hz\r\n"
    L"Bit depth: %ls\r\nChannels: %lu (%ls)\r\nTitle: %ls\r\nArtist: %ls\r\nAlbum: %ls\r\n"
    L"Modified: %04u-%02u-%02u %02u:%02u:%02u\r\nAttributes: %ls%ls%ls\r\nPlaying: %ls",
    L"Unknown", L"(none)", L"Read-only ", L"Hidden ", L"Archive", L"Normal", L"Yes", L"No",
    L"A file already exists at the destination:\n\n%ls\n\nOverwrite it?",
    L"Confirm overwrite", L"Select one or more files or folders in the source pane first.",
    L"A selected source or destination path is too long.",
    L"A selected source and destination are the same item.",
    L"A destination folder already exists:\n\n%ls\n\nContinuing will merge directory structures. "
    L"Windows will ask before replacing conflicting files. Continue?",
    L"Confirm folder merge", L"Move", L"Copy",
    L"Select one or more files or folders first.",
    L"Move %d selected item(s) to the Windows Recycle Bin?\n\n"
    L"WARNING: %d selected folder(s) may contain subdirectories and files. "
    L"Their entire contents will be deleted.",
    L"Move %d selected file(s) to the Windows Recycle Bin?", L"Confirm delete",
    L"Recycle Bin delete", L"New Folder", L"New Folder (%d)", L"New Folder...",
    L"The new folder path is too long.", L"Create folder",
    L"That folder could not be opened.", L"Folder", L"The selected path is too long.",
    L"Windows could not play this audio file.", L"Up", L"Refresh", L"Name", L"Size",
    L"Duration", L"Close", L"Application font and size:",
    L"Use monospace font (Consolas)", L"Remember window position and size",
    L"Interface size:", L"Window transparency:", L"Theme:", L"Language:", L"OK", L"Cancel",
    L"Interface size must be a number from 75 to 200 percent.", L"Invalid interface size",
    L"Window transparency must be a number from 0 to 50 percent.", L"Invalid transparency",
    L"Settings...", L"Info", L"Info \x25BC", L"Help", L"Copy ->", L"Move ->", L"Delete",
    L"Copy <-", L"Move <-", L"Swap", L"Stop",
    L"An item with that name already exists. Nothing was renamed.", L"Rename",
    L"AudioCommander is already running.",
    L"First-run notice",
    L"AudioCommander is provided \"as is\", without guarantees or warranties of any kind. "
    L"You use it entirely at your own risk. If data is lost, damaged, or anything else "
    L"goes wrong, you accept full responsibility.\n\n"
    L"Select Yes to accept these terms and continue. Select No to exit.",
    L"AudioCommander could not save your acceptance. Make sure the program folder is writable.",
    L"Permanently delete %d selected item(s) from this removable drive?\n\n"
    L"WARNING: This drive does not use the Windows Recycle Bin. %d selected folder(s) may "
    L"contain subfolders and files. Their entire contents will be permanently deleted. "
    L"This cannot be undone.",
    L"Permanently delete %d selected file(s) from this removable drive?\n\n"
    L"WARNING: This drive does not use the Windows Recycle Bin. This cannot be undone.",
    L"Confirm permanent delete", L"Permanent delete",
    L"Audio: %llu  :  %.1f MB  :  %llu:%02llu:%02llu",
    L"Audio: %llu  :  %.1f MB  :  %llu:%02llu:%02llu  :  %llu unknown",
    L"Browse...", L"Play next file automatically",
    L"Remember last used directories (left and right panes)",
    L"Copy progress",
    L"Copying to this drive is taking longer than usual. Please be patient; the computer is still working.",
    L"Copying file %d of %d: %ls",
    L"%.1f MB of %.1f MB (%u%%)"
};

static const wchar_t *const chinese[UI_COUNT] = {
    L"AudioCommander v9.3", L"AudioCommander 设置", L"AudioCommander v9.3 帮助",
    L"AudioCommander", L"未知的 Windows 错误。", L"%ls 失败。\n\n%ls",
    L"请选择音频文件以查看信息。", L"所选路径太长，无法显示。",
    L"文件夹\r\n名称：%ls\r\n路径：%ls", L"压缩编解码器不适用",
    L"名称：%ls\r\n路径：%ls\r\n扩展名：%ls\r\n容器：%ls\r\n编解码器：%ls\r\n"
    L"大小：%llu 字节（%llu KB）\r\n时长：%lu:%02lu.%03lu\r\n"
    L"比特率：%lu kbps\r\n比特率模式：%ls\r\n采样率：%lu Hz\r\n"
    L"位深度：%ls\r\n声道：%lu（%ls）\r\n标题：%ls\r\n艺术家：%ls\r\n专辑：%ls\r\n"
    L"修改时间：%04u-%02u-%02u %02u:%02u:%02u\r\n属性：%ls%ls%ls\r\n正在播放：%ls",
    L"未知", L"（无）", L"只读 ", L"隐藏 ", L"存档", L"普通", L"是", L"否",
    L"目标位置已存在文件：\n\n%ls\n\n是否覆盖？", L"确认覆盖",
    L"请先在源窗格中选择一个或多个文件或文件夹。", L"所选源路径或目标路径太长。",
    L"所选源和目标是同一项目。",
    L"目标文件夹已存在：\n\n%ls\n\n继续将合并文件夹结构。Windows 会在替换冲突文件前询问。是否继续？",
    L"确认合并文件夹", L"移动", L"复制", L"请先选择一个或多个文件或文件夹。",
    L"是否将选中的 %d 个项目移到 Windows 回收站？\n\n警告：选中的 %d 个文件夹可能包含子文件夹和文件，其全部内容都将被删除。",
    L"是否将选中的 %d 个文件移到 Windows 回收站？", L"确认删除", L"回收站删除",
    L"新建文件夹", L"新建文件夹 (%d)", L"新建文件夹...", L"新文件夹路径太长。",
    L"创建文件夹", L"无法打开该文件夹。", L"文件夹", L"所选路径太长。",
    L"Windows 无法播放此音频文件。", L"向上", L"刷新", L"名称", L"大小", L"时长",
    L"关闭", L"应用程序字体和大小：", L"使用等宽字体 (Consolas)",
    L"记住窗口位置和大小", L"界面大小：", L"窗口透明度：", L"主题：", L"语言：",
    L"确定", L"取消", L"界面大小必须是 75 到 200 之间的数字。", L"界面大小无效",
    L"窗口透明度必须是 0 到 50 之间的数字。", L"透明度无效",
    L"设置...", L"信息", L"信息 \x25BC", L"帮助", L"复制 ->", L"移动 ->", L"删除",
    L"复制 <-", L"移动 <-", L"交换", L"停止", L"已存在同名项目，未执行重命名。", L"重命名",
    L"AudioCommander 已在运行。",
    L"首次运行须知",
    L"AudioCommander 按“原样”提供，不作任何保证或担保。使用本程序的全部风险由您自行承担。"
    L"如果数据丢失、损坏或发生任何其他问题，您接受全部责任。\n\n"
    L"选择“是”表示接受这些条款并继续；选择“否”将退出程序。",
    L"AudioCommander 无法保存您的接受记录。请确保程序文件夹可写。",
    L"要从此可移动驱动器永久删除选中的 %d 个项目吗？\n\n"
    L"警告：此驱动器不使用 Windows 回收站。选中的 %d 个文件夹可能包含子文件夹和文件。"
    L"其中的全部内容将被永久删除，且无法撤销。",
    L"要从此可移动驱动器永久删除选中的 %d 个文件吗？\n\n"
    L"警告：此驱动器不使用 Windows 回收站。此操作无法撤销。",
    L"确认永久删除", L"永久删除",
    L"音频：%llu  :  %.1f MB  :  %llu:%02llu:%02llu",
    L"音频：%llu  :  %.1f MB  :  %llu:%02llu:%02llu  :  %llu 个时长未知",
    L"浏览...", L"自动播放下一个文件",
    L"记住上次使用的目录（左侧和右侧窗格）",
    L"复制进度",
    L"此复制操作比平时耗时更长。请耐心等待；计算机仍在工作。",
    L"正在复制第 %d 个文件（共 %d 个）：%ls",
    L"%.1f MB / %.1f MB（%u%%）"
};

static const wchar_t *const italian[UI_COUNT] = {
    L"AudioCommander v9.3", L"Impostazioni di AudioCommander", L"Guida di AudioCommander v9.3",
    L"AudioCommander", L"Errore Windows sconosciuto.", L"%ls non riuscito.\n\n%ls",
    L"Selezionare un file audio per visualizzarne le informazioni.",
    L"Il percorso selezionato è troppo lungo da visualizzare.",
    L"Cartella\r\nNome: %ls\r\nPercorso: %ls", L"Non applicabile al codec compresso",
    L"Nome: %ls\r\nPercorso: %ls\r\nEstensione: %ls\r\nContenitore: %ls\r\nCodec: %ls\r\n"
    L"Dimensione: %llu byte (%llu KB)\r\nDurata: %lu:%02lu.%03lu\r\n"
    L"Bitrate: %lu kbps\r\nModalità bitrate: %ls\r\nFrequenza: %lu Hz\r\n"
    L"Profondità: %ls\r\nCanali: %lu (%ls)\r\nTitolo: %ls\r\nArtista: %ls\r\nAlbum: %ls\r\n"
    L"Modificato: %04u-%02u-%02u %02u:%02u:%02u\r\nAttributi: %ls%ls%ls\r\nIn riproduzione: %ls",
    L"Sconosciuto", L"(nessuno)", L"Sola lettura ", L"Nascosto ", L"Archivio", L"Normale", L"Sì", L"No",
    L"Esiste già un file nella destinazione:\n\n%ls\n\nSovrascriverlo?", L"Conferma sovrascrittura",
    L"Selezionare prima uno o più file o cartelle nel riquadro sorgente.",
    L"Un percorso sorgente o destinazione selezionato è troppo lungo.",
    L"La sorgente e la destinazione selezionate sono lo stesso elemento.",
    L"Esiste già una cartella di destinazione:\n\n%ls\n\nContinuando verranno unite le strutture. "
    L"Windows chiederà conferma prima di sostituire i file in conflitto. Continuare?",
    L"Conferma unione cartella", L"Spostamento", L"Copia",
    L"Selezionare prima uno o più file o cartelle.",
    L"Spostare %d elementi selezionati nel Cestino di Windows?\n\n"
    L"ATTENZIONE: %d cartelle selezionate possono contenere sottocartelle e file. Tutto il contenuto verrà eliminato.",
    L"Spostare %d file selezionati nel Cestino di Windows?", L"Conferma eliminazione",
    L"Eliminazione nel Cestino", L"Nuova cartella", L"Nuova cartella (%d)", L"Nuova cartella...",
    L"Il percorso della nuova cartella è troppo lungo.", L"Creazione cartella",
    L"Impossibile aprire la cartella.", L"Cartella", L"Il percorso selezionato è troppo lungo.",
    L"Windows non ha potuto riprodurre questo file audio.", L"Su", L"Aggiorna", L"Nome",
    L"Dimensione", L"Durata", L"Chiudi", L"Carattere e dimensione dell'applicazione:",
    L"Usa carattere monospaziato (Consolas)", L"Ricorda posizione e dimensione finestra",
    L"Dimensione interfaccia:", L"Trasparenza finestra:", L"Tema:", L"Lingua:", L"OK", L"Annulla",
    L"La dimensione dell'interfaccia deve essere un numero da 75 a 200.",
    L"Dimensione interfaccia non valida",
    L"La trasparenza della finestra deve essere un numero da 0 a 50.", L"Trasparenza non valida",
    L"Impostazioni...", L"Info", L"Info \x25BC", L"Guida", L"Copia ->", L"Sposta ->", L"Elimina",
    L"Copia <-", L"Sposta <-", L"Scambia", L"Ferma",
    L"Esiste già un elemento con questo nome. Nessuna rinomina eseguita.", L"Rinomina",
    L"AudioCommander è già in esecuzione.",
    L"Avviso al primo avvio",
    L"AudioCommander viene fornito \"così com'è\", senza garanzie di alcun tipo. "
    L"L'uso avviene interamente a proprio rischio. In caso di perdita o danneggiamento "
    L"dei dati o di qualsiasi altro problema, l'utente accetta la piena responsabilità.\n\n"
    L"Selezionare Sì per accettare e continuare. Selezionare No per uscire.",
    L"AudioCommander non ha potuto salvare l'accettazione. Verificare che la cartella del programma sia scrivibile.",
    L"Eliminare definitivamente %d elementi selezionati da questa unità rimovibile?\n\n"
    L"ATTENZIONE: questa unità non usa il Cestino di Windows. Le %d cartelle selezionate "
    L"possono contenere sottocartelle e file. Tutto il contenuto verrà eliminato "
    L"definitivamente. L'operazione non può essere annullata.",
    L"Eliminare definitivamente %d file selezionati da questa unità rimovibile?\n\n"
    L"ATTENZIONE: questa unità non usa il Cestino di Windows. L'operazione non può essere annullata.",
    L"Conferma eliminazione definitiva", L"Eliminazione definitiva",
    L"Audio: %llu  :  %.1f MB  :  %llu:%02llu:%02llu",
    L"Audio: %llu  :  %.1f MB  :  %llu:%02llu:%02llu  :  %llu durata sconosciuta",
    L"Sfoglia...", L"Riproduci automaticamente il file successivo",
    L"Ricorda le ultime cartelle usate (riquadro sinistro e destro)",
    L"Avanzamento copia",
    L"La copia sta richiedendo più tempo del solito. Attendere; il computer sta ancora lavorando.",
    L"Copia del file %d di %d: %ls",
    L"%.1f MB di %.1f MB (%u%%)"
};

static const wchar_t *const polish[UI_COUNT] = {
    L"AudioCommander v9.3", L"Ustawienia AudioCommander", L"Pomoc AudioCommander v9.3",
    L"AudioCommander", L"Nieznany błąd systemu Windows.", L"Operacja %ls nie powiodła się.\n\n%ls",
    L"Wybierz plik audio, aby wyświetlić informacje.", L"Wybrana ścieżka jest zbyt długa do wyświetlenia.",
    L"Folder\r\nNazwa: %ls\r\nŚcieżka: %ls", L"Nie dotyczy kodeka skompresowanego",
    L"Nazwa: %ls\r\nŚcieżka: %ls\r\nRozszerzenie: %ls\r\nKontener: %ls\r\nKodek: %ls\r\n"
    L"Rozmiar: %llu bajtów (%llu KB)\r\nCzas: %lu:%02lu.%03lu\r\n"
    L"Przepływność: %lu kbps\r\nTryb przepływności: %ls\r\nPróbkowanie: %lu Hz\r\n"
    L"Głębia bitowa: %ls\r\nKanały: %lu (%ls)\r\nTytuł: %ls\r\nWykonawca: %ls\r\nAlbum: %ls\r\n"
    L"Zmodyfikowano: %04u-%02u-%02u %02u:%02u:%02u\r\nAtrybuty: %ls%ls%ls\r\nOdtwarzanie: %ls",
    L"Nieznane", L"(brak)", L"Tylko do odczytu ", L"Ukryty ", L"Archiwalny", L"Normalny", L"Tak", L"Nie",
    L"Plik już istnieje w miejscu docelowym:\n\n%ls\n\nZastąpić go?", L"Potwierdź zastąpienie",
    L"Najpierw wybierz co najmniej jeden plik lub folder w panelu źródłowym.",
    L"Wybrana ścieżka źródłowa lub docelowa jest zbyt długa.",
    L"Wybrane źródło i miejsce docelowe są tym samym elementem.",
    L"Folder docelowy już istnieje:\n\n%ls\n\nKontynuacja scali struktury folderów. "
    L"Windows zapyta przed zastąpieniem konfliktowych plików. Kontynuować?",
    L"Potwierdź scalenie folderów", L"Przenoszenie", L"Kopiowanie",
    L"Najpierw wybierz co najmniej jeden plik lub folder.",
    L"Przenieść %d zaznaczonych elementów do Kosza systemu Windows?\n\n"
    L"OSTRZEŻENIE: %d zaznaczonych folderów może zawierać podfoldery i pliki. Cała zawartość zostanie usunięta.",
    L"Przenieść %d zaznaczonych plików do Kosza systemu Windows?", L"Potwierdź usunięcie",
    L"Usuwanie do Kosza", L"Nowy folder", L"Nowy folder (%d)", L"Nowy folder...",
    L"Ścieżka nowego folderu jest zbyt długa.", L"Tworzenie folderu",
    L"Nie można otworzyć tego folderu.", L"Folder", L"Wybrana ścieżka jest zbyt długa.",
    L"System Windows nie mógł odtworzyć tego pliku audio.", L"W górę", L"Odśwież", L"Nazwa",
    L"Rozmiar", L"Czas", L"Zamknij", L"Czcionka i rozmiar aplikacji:",
    L"Użyj czcionki o stałej szerokości (Consolas)", L"Zapamiętaj położenie i rozmiar okna",
    L"Rozmiar interfejsu:", L"Przezroczystość okna:", L"Motyw:", L"Język:", L"OK", L"Anuluj",
    L"Rozmiar interfejsu musi być liczbą od 75 do 200.", L"Nieprawidłowy rozmiar interfejsu",
    L"Przezroczystość okna musi być liczbą od 0 do 50.", L"Nieprawidłowa przezroczystość",
    L"Ustawienia...", L"Info", L"Info \x25BC", L"Pomoc", L"Kopiuj ->", L"Przenieś ->", L"Usuń",
    L"Kopiuj <-", L"Przenieś <-", L"Zamień", L"Stop",
    L"Element o tej nazwie już istnieje. Nazwa nie została zmieniona.", L"Zmiana nazwy",
    L"AudioCommander jest już uruchomiony.",
    L"Informacja przy pierwszym uruchomieniu",
    L"AudioCommander jest dostarczany „w stanie, w jakim jest”, bez jakichkolwiek "
    L"gwarancji lub zapewnień. Użytkownik korzysta z programu wyłącznie na własne "
    L"ryzyko. W przypadku utraty lub uszkodzenia danych albo wystąpienia innego "
    L"problemu użytkownik przyjmuje pełną odpowiedzialność.\n\n"
    L"Wybierz Tak, aby zaakceptować te warunki i kontynuować. Wybierz Nie, aby zakończyć.",
    L"AudioCommander nie mógł zapisać akceptacji. Upewnij się, że folder programu jest zapisywalny.",
    L"Trwale usunąć %d zaznaczonych elementów z tego dysku wymiennego?\n\n"
    L"OSTRZEŻENIE: ten dysk nie używa Kosza systemu Windows. %d zaznaczonych folderów "
    L"może zawierać podfoldery i pliki. Cała ich zawartość zostanie trwale usunięta. "
    L"Tej operacji nie można cofnąć.",
    L"Trwale usunąć %d zaznaczonych plików z tego dysku wymiennego?\n\n"
    L"OSTRZEŻENIE: ten dysk nie używa Kosza systemu Windows. Tej operacji nie można cofnąć.",
    L"Potwierdź trwałe usunięcie", L"Trwałe usuwanie",
    L"Audio: %llu  :  %.1f MB  :  %llu:%02llu:%02llu",
    L"Audio: %llu  :  %.1f MB  :  %llu:%02llu:%02llu  :  %llu bez czasu",
    L"Przeglądaj...", L"Automatycznie odtwarzaj następny plik",
    L"Zapamiętaj ostatnio używane foldery (lewy i prawy panel)",
    L"Postęp kopiowania",
    L"Kopiowanie trwa dłużej niż zwykle. Proszę czekać; komputer nadal pracuje.",
    L"Kopiowanie pliku %d z %d: %ls",
    L"%.1f MB z %.1f MB (%u%%)"
};

static const wchar_t help_english[] =
    L"BASIC PLAYBACK\r\nClick an audio file once to play it. Click it again or press Stop to stop. "
    L"Click the progress bar to seek and use the volume slider to adjust playback. Enable Play next file "
    L"automatically in Settings to continue through the initiating pane's current order.\r\n\r\n"
    L"BROWSING AND FILES\r\nBoth panes navigate independently. Use Up, a drive selector, Browse, or type a path. "
    L"Use Ctrl or Shift for multiple selection. Copy and Move send items in the arrow direction. Delete moves "
    L"items on normal drives to the Recycle Bin after one confirmation. Removable drives do not use the Recycle "
    L"Bin, so deletion there is permanent and requires a separate warning with No as the default. "
    L"Stop halts all playback, Swap exchanges the folders, and "
    L"Refresh reloads both panes. Click Name, Size, or Duration to sort; click again to reverse the order.\r\n\r\n"
    L"KEYBOARD\r\nEnter plays or stops the focused file. F1 opens Help; F2 copies, F3 moves, F4 deletes, and F5 "
    L"refreshes the focused pane. Right-click a list to create a folder.\r\n\r\n"
    L"APPEARANCE\r\nSettings controls font, interface size, transparency, theme, language, remembered window "
    L"position, and whether both panes reopen their last folders. Pastel is the default; Windows Native is the other built-in choice. External .skn palettes are loaded from the skins folder. Missing or invalid skins fall back to Pastel. Settings are stored in "
    L"audiocommander.ini beside the program.\r\n\r\n"
    L"FORMATS\r\nWAV, MP3, M4A, MP4 audio, FLAC, WMA, Ogg Vorbis, Opus, ALAC, MIDI (.mid), and tracker modules "
    L"(.mod, .s3m, .xm) are supported. Tracker decoding uses bundled static libopenmpt. Other formats require their "
    L"Windows or bundled FFmpeg decoder. This software uses FFmpeg libraries under LGPL 2.1 or later.";

static const wchar_t help_chinese[] =
    L"基本播放\r\n单击音频文件开始播放；再次单击或按“停止”结束播放。单击进度条可跳转，使用音量滑块调节音量。"
    L"在“设置”中启用“自动播放下一个文件”，可按开始播放时窗格中的当前顺序继续播放。\r\n\r\n"
    L"浏览和文件\r\n左右窗格可独立浏览。可使用“向上”、驱动器选择器、“浏览”或直接输入路径。按 Ctrl 或 Shift 可多选。"
    L"复制和移动按箭头方向执行；普通驱动器上的删除经一次确认后移入回收站。可移动驱动器不使用回收站，"
    L"因此删除是永久性的，并会显示单独警告且默认选择“否”。“停止”结束全部播放，“交换”互换两个文件夹，"
    L"“刷新”重新载入两个窗格。单击名称、大小或时长标题进行排序，再次单击可反向排序。\r\n\r\n"
    L"键盘\r\nEnter 播放或停止焦点文件；F1 打开帮助，F2 复制，F3 移动，F4 删除，F5 刷新焦点窗格。右键单击列表可新建文件夹。\r\n\r\n"
    L"外观\r\n设置可调整字体、界面大小、透明度、主题、语言、窗口位置，以及左右窗格是否重新打开上次使用的目录。柔和色是默认主题，Windows 原生是另一个内置选项；外部 .skn 调色板从程序旁的 skins 文件夹加载，缺失或无效时回退到柔和色。"
    L"设置保存在程序旁的 audiocommander.ini 中。\r\n\r\n"
    L"格式\r\n支持 WAV、MP3、M4A、MP4 音频、FLAC、WMA、Ogg Vorbis、Opus、ALAC、MIDI (.mid)，以及跟踪器模块 MOD (.mod)、S3M (.s3m) 和 XM (.xm)。"
    L"跟踪器解码器 libopenmpt 已静态内置；其他格式需要相应的 Windows 或随附 FFmpeg 解码器。"
    L"本软件依照 LGPL 2.1 或更高版本使用 FFmpeg 库。";

static const wchar_t help_italian[] =
    L"RIPRODUZIONE\r\nFare clic su un file audio per riprodurlo; fare di nuovo clic o premere Ferma per arrestarlo. "
    L"Fare clic sulla barra di avanzamento per spostarsi e usare il cursore del volume. Attivare la riproduzione "
    L"automatica del file successivo nelle Impostazioni per continuare nell'ordine corrente del riquadro iniziale.\r\n\r\n"
    L"ESPLORAZIONE E FILE\r\nI due riquadri navigano in modo indipendente. Usare Su, il selettore unità, Sfoglia o digitare "
    L"un percorso. Ctrl o Maiusc selezionano più elementi. Copia e Sposta seguono la freccia; Elimina sposta nel "
    L"Cestino dopo una conferma. Le unità rimovibili non usano il Cestino: l'eliminazione è definitiva e richiede "
    L"un avviso separato con No come scelta predefinita. Ferma arresta ogni riproduzione, Scambia scambia le cartelle e Aggiorna ricarica "
    L"entrambi i riquadri. Fare clic su Nome, Dimensione o Durata per ordinare e di nuovo per invertire.\r\n\r\n"
    L"TASTIERA\r\nInvio riproduce o arresta il file attivo. F1 apre la Guida; F2 copia, F3 sposta, F4 elimina e F5 "
    L"aggiorna il riquadro attivo. Il clic destro crea una cartella.\r\n\r\n"
    L"ASPETTO\r\nImpostazioni controlla carattere, dimensione interfaccia, trasparenza, tema, lingua, posizione "
    L"della finestra e riapertura delle ultime cartelle nei due riquadri. Pastello è il tema predefinito e Windows nativo è l'altra scelta incorporata. Le tavolozze .skn esterne sono caricate dalla cartella skins; quelle mancanti o non valide tornano a Pastello. Le scelte sono salvate in "
    L"audiocommander.ini accanto al programma.\r\n\r\n"
    L"FORMATI\r\nSono supportati WAV, MP3, M4A, audio MP4, FLAC, WMA, Ogg Vorbis, Opus, ALAC, MIDI (.mid) e moduli tracker (.mod, .s3m, .xm) quando è disponibile "
    L"il decoder Windows o FFmpeg richiesto. Le librerie FFmpeg sono usate secondo LGPL 2.1 o successiva.";

static const wchar_t help_polish[] =
    L"ODTWARZANIE\r\nKliknij plik audio, aby go odtworzyć; kliknij ponownie lub naciśnij Stop, aby zatrzymać. "
    L"Kliknięcie paska postępu przewija, a suwak reguluje głośność. Włącz automatyczne odtwarzanie następnego "
    L"pliku w Ustawieniach, aby kontynuować według bieżącej kolejności panelu początkowego.\r\n\r\n"
    L"PRZEGLĄDANIE I PLIKI\r\nPanele działają niezależnie. Użyj W górę, wyboru dysku, Przeglądaj albo wpisz ścieżkę. Ctrl lub "
    L"Shift zaznacza wiele elementów. Kopiowanie i przenoszenie działa zgodnie ze strzałką; usuwanie przenosi do "
    L"Kosza po jednym potwierdzeniu. Dyski wymienne nie używają Kosza, dlatego usuwanie jest trwałe i wymaga "
    L"osobnego ostrzeżenia z domyślną odpowiedzią Nie. Stop zatrzymuje dźwięk, Zamień wymienia foldery, a Odśwież przeładowuje oba "
    L"panele. Kliknij Nazwa, Rozmiar lub Czas, aby sortować; ponowne kliknięcie odwraca kolejność.\r\n\r\n"
    L"KLAWIATURA\r\nEnter odtwarza lub zatrzymuje aktywny plik. F1 otwiera Pomoc, F2 kopiuje, F3 przenosi, F4 usuwa, "
    L"a F5 odświeża aktywny panel. Kliknij listę prawym przyciskiem, aby utworzyć folder.\r\n\r\n"
    L"WYGLĄD\r\nUstawienia obejmują czcionkę, rozmiar interfejsu, przezroczystość, motyw, język, położenie okna "
    L"oraz ponowne otwieranie ostatnich folderów w obu panelach. "
    L"Pastelowy jest motywem domyślnym, a Natywny Windows drugim motywem wbudowanym. Zewnętrzne palety .skn są ładowane z folderu skins; brakujące lub nieprawidłowe wracają do Pastelowego. Ustawienia są w audiocommander.ini obok programu.\r\n\r\n"
    L"FORMATY\r\nObsługiwane są WAV, MP3, M4A, audio MP4, FLAC, WMA, Ogg Vorbis, Opus, ALAC, MIDI (.mid) i moduły tracker (.mod, .s3m, .xm), jeśli dostępny jest "
    L"wymagany dekoder Windows lub FFmpeg. Biblioteki FFmpeg są używane na licencji LGPL 2.1 lub nowszej.";

const wchar_t *localization_text(AppLanguage language, UiText text)
{
    const wchar_t *const *table = english;
    if (text < 0 || text >= UI_COUNT) return L"";
    if (language == APP_LANGUAGE_CHINESE_SIMPLIFIED) table = chinese;
    else if (language == APP_LANGUAGE_ITALIAN) table = italian;
    else if (language == APP_LANGUAGE_POLISH) table = polish;
    if (table[text] == NULL || table[text][0] == L'\0') return english[text];
    return table[text];
}

const wchar_t *localization_help_text(AppLanguage language)
{
    if (language == APP_LANGUAGE_CHINESE_SIMPLIFIED) return help_chinese;
    if (language == APP_LANGUAGE_ITALIAN) return help_italian;
    if (language == APP_LANGUAGE_POLISH) return help_polish;
    return help_english;
}

const wchar_t *localization_theme_name(AppLanguage language, AppTheme theme)
{
    if (language < 0 || language >= APP_LANGUAGE_COUNT) language = APP_LANGUAGE_ENGLISH;
    if (theme < 0 || theme >= APP_THEME_COUNT) theme = APP_THEME_PASTEL;
    if (theme == APP_THEME_PASTEL) {
        if (language == APP_LANGUAGE_CHINESE_SIMPLIFIED)
            return L"\x67D4\x548C\x8272";
        if (language == APP_LANGUAGE_ITALIAN) return L"Pastello";
        if (language == APP_LANGUAGE_POLISH) return L"Pastelowy";
    }
    if (theme == APP_THEME_WINDOWS_NATIVE) {
        if (language == APP_LANGUAGE_CHINESE_SIMPLIFIED)
            return L"Windows \x539F\x751F";
        if (language == APP_LANGUAGE_ITALIAN) return L"Windows nativo";
        if (language == APP_LANGUAGE_POLISH) return L"Natywny Windows";
    }
    switch (theme) {
    case APP_THEME_ACRYL: return L"Acryl";
    case APP_THEME_AIR: return L"Air";
    case APP_THEME_METRO_UI: return L"MetroUI";
    case APP_THEME_OFFICE_2003_SKIN: return L"Office 2003";
    case APP_THEME_OFFICE_2007_BLACK: return L"Office 2007 Black";
    case APP_THEME_XP_LUNA: return L"XP Luna";
    case APP_THEME_XP_SILVER: return L"XP Silver";
    case APP_THEME_ZEST: return L"Zest";
    default: return L"Pastel";
    }
}

bool localization_language_complete(AppLanguage language)
{
    const wchar_t *const *table;
    int index;
    if (language < 0 || language >= APP_LANGUAGE_COUNT) return false;
    table = language == APP_LANGUAGE_CHINESE_SIMPLIFIED ? chinese :
            language == APP_LANGUAGE_ITALIAN ? italian :
            language == APP_LANGUAGE_POLISH ? polish : english;
    for (index = 0; index < UI_COUNT; ++index)
        if (table[index] == NULL || table[index][0] == L'\0') return false;
    return localization_help_text(language)[0] != L'\0';
}
