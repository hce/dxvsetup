-- Copyright (c) 2012, Hans-Christian Esperer
-- All rights reserved.
-- 
-- Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
-- 
-- Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
-- Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
-- THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

OUR_VERSION               = 0 -- TODO: revision number goes here. Start counting from 1, integers only
HASH_MODFILESTXT          = "" -- TODO: SHA1HASH
HASH_NEWFILESTXT          = "" -- TODO: SHA1HASH
HASH_TOBEPATCHEDTXT       = "" -- TODO: SHA1HASH
HASH_DIRECTORIESTXT       = "" -- TODO: SHA1HASH
OneMegabyte               = 1024 * 1024 * 1024
HalfAGigabyte             = OneMegabyte * 512
Mirrors                   = {-- TODO: List of mirrors-- TODO: List of mirrors-- TODO: List of mirrors-- TODO: List of mirrors
	{host="",        port=80, prefix="dxveteryfiles"}
				   }
Hostname                  = nil
Hostport                  = nil
UrlPrefix                 = nil
InstallDst                = "C:\\DeusEx\\DXVetery"
InstallSrc                = "C:\\DeusEx"
txtGameName               = "DEFAULT GAME NAME -- REPORT THIS AS A BUG"

msgWelcome                = [[TODO]]
msgLicenseFoo             = [[TODO]]
msgChoosePaths            = [[TODO]]
msgInstallationFinished   = [[TODO]]
msgError                  = [[TODO %s]]
msgInstallationCanceled   = [[TODO]]
msgInstallationReady      = [[TODO]]
msgInstallationCReady     = [[TODO]] -- Interrupted installation will be continued
msgNoPatchFound           = [[TODO]]
msgDirectoryExists        = [[TODO]]
msgSteamVersion           = [[Steam problem message]]


PathJoin = function(PathLeft, PathRight)
	if string.sub(PathLeft, -1) == "\\" then
		return PathLeft .. PathRight
	else
		return PathLeft .. "\\" .. PathRight
	end
end

JoinUrl = function(UrlLeft, UrlRight)
	if string.sub(UrlLeft, -1) == "/" then
		return UrlLeft .. UrlRight
	else
		return UrlLeft .. "/" .. UrlRight
	end
end

makeList = function(urlPath, listHash, listType)
	print("Fetching " .. listType)
	sList = FetchHTTP(Hostname, Hostport, JoinUrl(UrlPrefix, urlPath), OneMegabyte)
	print("Verifying list's sha1 hash value")
	actualListsHash = SHA1(sList)
	if actualListsHash ~= listHash then
		error(string.format("Verifying the integrity of %s failed! (Expected %s, got %s)", listType, listHash, actualListsHash))
	end
	sList = string.gsub(sList, "/", "\\")
	lList = {}
	for filename, filehash in string.gmatch(sList, "([^\n]+) ([^ ]+)\n") do
		lList[filename] = filehash
	end
	return lList
end

createDirectories = function(urlpath, correctHash, listType)
        dirlist = FetchHTTP(Hostname, Hostport, JoinUrl(UrlPrefix, urlpath), OneMegabyte)
        actualHash = SHA1(dirlist)
        if actualHash ~= correctHash then
                error(string.format("Verifying the integrity of %s failed! (Expected %s, got %s)", listType, correctHash, actualHash))
        end
	for dirname in string.gmatch(dirlist, "([^\n]+)\n") do
		dirtocreate = PathJoin(InstallDst, dirname)
		print(string.format("mkdir %s", dirtocreate))
		CreateDirectory(dirtocreate)
	end
end

doSteamCheck = function()
	print("Doing steam check")
	local pathtosystem = PathJoin(InstallSrc, "System")
	local deusexexe_hash = HashFile(PathJoin(pathtosystem, "DeusEx.exe"))
	if deusexexe_hash == "af951ddd35b38e8d9cc8501b8a50a02a3ab6cae7" then
		local dxaltexists, deusexalt_hash = pcall(HashFile, PathJoin(pathtosystem, "DeusEx.alt"))
		if dxaltexists and (deusexalt_hash ~= deusexexe_hash) then
			return true
		elseif dxaltexists then
			MessageBox(txtGameName, "Invalid DeusEx.alt file", "error")
			-- no return here; show dialogue again
		end
		if not ShowTextDialog(txtGameName .. " - STEAM causes problems", string.gsub(string.format(msgSteamVersion, pathtosystem, pathtosystem), "\n", "\r\n")) then
			error("Installation of " .. txtGameName .. " was aborted at the user's request")
		end
		return false
	end
	return true
end

doVersionCheck = function()
	local pathtosystem = PathJoin(InstallSrc, "System")
	local deusexu_hash = HashFile(PathJoin(pathtosystem, "DeusEx.u"))
	if deusexu_hash ~= "f32766c053ba1cc6ed408bcebe065470e4487574" then
		MessageBox(txtGameName, "Your version of DeusEx.u is incompatible with this mod. Please install DeusEx version 1.112fm. If you need help, contact hc-dxv@hcesperer.org . Thanks!", "error")
		return false
	else
		return true
	end
end

checkDirectory = function()
	if not Exists(PathJoin(PathJoin(InstallSrc, "System"), "DeusEx.exe")) then
		MessageBox("DXVSetup", "The original game was not found at the specified directory.", "error")
		return false, false
	end

	if not doSteamCheck() then
		return false, false
	end

	if not doVersionCheck() then
		return false, false
	end

	partinst, errormsg = io.open(PathJoin(InstallDst, "pinstall.txt"), "r")
	if partinst ~= nil then
		-- MessageBox("DXVSetup", "You are about to continue a partially finished installation.", "information")
		io.close(partinst)
		return true, true
	end
	
	if Exists(InstallDst) then
		if not IsDir(InstallDst) then
			MessageBox("DXVSetup", "You have specified an invalid installation directory.", "error")
			return false, false
		else
		    if DeleteDirectory(InstallDst) then
				-- Dir was empty, re-create it
				CreateDirectory(InstallDst)
				return true, false
			else
				ShowTextDialog(txtGameName .. " - Cannot continue", string.gsub(string.format(msgDirectoryExists, InstallDst), "\n", "\r\n"))
				return false, false
			end
		end
	else
		if MessageBox("DXVSetup", string.format("The installation directory %s does not exist. Create it?", InstallDst), "question") then
			if CreateDirectory(InstallDst) then
				return true, false
			else
				MessageBox("DXVSetup", "Unable to create installation directory", "error")
				return false, false
			end
		else
			return false, false
		end
	end
end

directoryHeuristics = function()
	print("Trying to determine path of existing Deus Ex installation")
	reqsucc, dxinst = pcall(GetRegKey, "HKEY_CLASSES_ROOT", "DeusEx.Map\\Shell\\open\\command", nil)
	if (not reqsucc) or (dxinst == nil) then
		print("Unsuccessful.")
		MessageBox("DXVSetup", "Unable to find your installed copy of Deus Ex. You have to manually specify where you have Deus Ex installed.", "information")
	else
		dxinst = string.match(dxinst, "([^ ]+)\\[Ss][Yy][Ss][Tt][Ee][Mm]\\[Dd][Ee][Uu][Ss][Ee][Xx].[Ee][Xx][Ee]")
		print(string.format("Success: found it at %s.", dxinst))
		InstallSrc = dxinst
	end

	InstallDst = PathJoin(GetUserDir("CSIDL_PROFILE"), "DeusExVetery")
end

findMirror = function()
    for i, mirror in pairs(Mirrors) do
		Hostname  = mirror.host
		Hostport  = mirror.port
		UrlPrefix = mirror.prefix
		print(string.format("Trying mirror #%d: %s:%d", i, Hostname, Hostport))
		success, version = pcall(FetchHTTP, Hostname, Hostport, JoinUrl(UrlPrefix, "version.yaws"), OneMegabyte)
		if success and (version ~= nil) then
			print(string.format("Mirror %s it is.", Hostname))
			return true
		else
			print(string.format("Mirror %d not usable: %s", i, version))
		end
	end
	return false
end

setupRoutine = function()
	print("User input required (confirm/cancel at welcome message)")
	if not ShowTextDialog(txtGameName .. " - Welcome", string.gsub(msgWelcome, "\n", "\r\n")) then
		MessageBox("DXVSetup", "You have chosen not to continue with the installation. Installation is thus aborted.", "information")
		return false
	end
	if not ShowTextDialog(txtGameName .. " - License foo", string.gsub(msgLicenseFoo, "\n", "\r\n")) then
		MessageBox("DXVSetup", "You have chosen not to continue with the installation. Installation is thus aborted.", "information")
		return false
	end

	directoryHeuristics()

	print("Selecting mirror")
	if not findMirror() then
		MessageBox("DXVSetup", "Could not reach any mirror servers. Please check your connection to the Aquinas Hub and if in doubt, visit http://deusex.hcesperer.org/ for up to date information", "error")
		return false
	end

	print("Checking if installer is up to date")
	version = FetchHTTP(Hostname, Hostport, JoinUrl(UrlPrefix, "version.yaws"), OneMegabyte)
	version = tonumber(string.match(version, "[0-9]+"))
	if version > OUR_VERSION then
		error("You run an old version of the installer. Please go to http://deusex.hcesperer.org/ and download a newer version of dxvsetup.exe. Sorry for the inconvenience!")
	elseif version < OUR_VERSION then
		error("Apparently the mirror is serving too old files. Can't continue. Please fetch an updated installer at http://deusex.hcesperer.org/")
	end

	print("User input required (choose paths)")
	local partinst
	while true do
		local success
		if not ShowFileDialog(txtGameName .. " - Select paths", string.gsub(msgChoosePaths, "\n", "\r\n")) then
			MessageBox("DXVSetup", "You have aborted the installation. If you had any trouble specifying the directories, please visit http://deusex.hcesperer.org/ for contact information.", "information")
			return false
		end
		InstallDst = PathJoin(InstallSrc, "DXVetery")
		success, partinst = checkDirectory()
		if success then
			break
		end
	end

	pprint("Initialising setup")
	print("Fetching list of modfiles")
	modfiles = makeList("modfiles.txt", HASH_MODFILESTXT, "List of mod files")
	print("Fetching list of new files")
	newfiles = makeList("newfiles.txt", HASH_NEWFILESTXT, "List of new files")
	print("Fetching list of patches")
	patches  = makeList("tobepatched.txt", HASH_TOBEPATCHEDTXT, "List of patches")

	print("Creating directories")
	createDirectories("directories.txt", HASH_DIRECTORIESTXT, "List of directories")

	print("Creating lockfile")
	partinstfile, possibleerror = io.open(PathJoin(InstallDst, "pinstall.txt"), "w")
	if partinstfile == nil then
		error("Unable to create marker file")
	end

	partinstfile:write("Installing " .. txtGameName)
	partinstfile:close()

	pprint("Ready to install")
	print("User input required (start actual installation process)")
	
	if partinst then
		instReadyMsg = msgInstallationCReady
	else
		instReadyMsg = msgInstallationReady
	end
	instReadyMsg = string.format(instReadyMsg, InstallDst)

	if not ShowTextDialog(txtGameName .. " - Ready to install", string.gsub(instReadyMsg, "\n", "\r\n")) then
		MessageBox("DXVSetup", "You have chosen not to continue with the installation. Installation is thus aborted.", "information")
		return false
	end

	MessageBox("DXVSetup", "You are installing the DXVetery Beta Demo. Please report any bugs to hc-dxv@hcesperer.org; Thanks very much in advance!", "information")
	
	pprint("Installing Deus Ex Vetery")
	for modfile, modhash in pairs(modfiles) do
		print(string.format("Checking %s", modfile))
		fileexists, filehash = pcall(HashFile, PathJoin(InstallDst, modfile))
		if fileexists and (filehash == modhash) then
			print(string.format("%s already exists (correct hash)", modfile))
		else
			nf = newfiles[modfile]
			pf = patches[modfile]

			dopatch = (pf ~= nil)
			localcopy = false

			-- If the file has a patch, nevertheless check if
			-- the local version really needs patching
			if dopatch then
				print("Checking if patching is really required")
				fileexistslocally, tbpatchedfilehash = pcall(HashFile, PathJoin(InstallSrc, modfile))
				if fileexistslocally and (tbpatchedfilehash == modhash) then
					print(string.format("%s has a patch available, but patching is not required", modfile))
					dopatch = false
				end
			end

			-- Is it a file that *must* be downloaded in its entirety?
			-- (i.e., a file that is not present in the original game
			--  or one that is going to be replaced fully in every case)
			if nf ~= nil then
				print(string.format("Downloading %s [bz2]", modfile))
				bz2testsucc, resulttoignore = pcall(FetchHTTP, Hostname, Hostport, JoinUrl(UrlPrefix, string.format("%s.new.bz2", modhash)),
					HalfAGigabyte, PathJoin(InstallDst, modfile .. ".bz2"))
				if bz2testsucc then
					print(string.format("bzip2 decompressing %s", modfile))
					CopyDecompress(PathJoin(InstallDst, modfile .. ".bz2"), PathJoin(InstallDst, modfile) .. ".notverifiedyet")
					pcall(os.remove, PathJoin(InstallDst, modfile .. ".bz2"))
				else
					print(string.format("Downloading %s [precompressed]", modfile))
					FetchHTTP(Hostname, Hostport, JoinUrl(UrlPrefix, string.format("%s.new", modhash)),
						HalfAGigabyte, PathJoin(InstallDst, modfile) .. ".notverifiedyet")
				end

			-- Is it a file that must be patched?
			elseif dopatch then
				print(string.format("Downloading patch for %s", modfile))
				patchFile = GetTempFN("somepatch")
				srcfilehash = HashFile(PathJoin(InstallSrc, modfile))
				successfulfetch, ignoreresult = pcall(FetchHTTP, Hostname, Hostport, JoinUrl(UrlPrefix, string.format("%s.patch", srcfilehash)),
					HalfAGigabyte, patchFile)

				if not successfulfetch then
					ShowTextDialog(txtGameName .. " - Patch not found", string.gsub(msgNoPatchFound, "\n", "\r\n"))
					error("Installation cannot continue")
				end

				print(string.format("Applying patch to %s", modfile))
				PatchFile(PathJoin(InstallSrc, modfile),
					  PathJoin(InstallDst, modfile) .. ".notverifiedyet",
					  patchFile)

				print(string.format("Deleting patch file %s", patchFile))
				os.remove(patchFile)
			else
				print(string.format("Copying %s unmodified", modfile))
				if (string.lower(modfile) == "system\\deusex.exe") and Exists(PathJoin(InstallSrc, "system\\deusex.alt")) then
					print("Using DeusEx.ALT instead of DeusEx.EXE")
					CopyFile(PathJoin(InstallSrc, "system\\deusex.alt"),
						PathJoin(InstallDst, modfile) .. ".notverifiedyet")
				else
					CopyFile(PathJoin(InstallSrc, modfile),
						PathJoin(InstallDst, modfile) .. ".notverifiedyet")
				end
				localcopy = true
			end

			print(string.format("Verifying %s", modfile))
			localhash = HashFile(PathJoin(InstallDst, modfile) .. ".notverifiedyet")
			if localhash ~= modhash then
				if localcopy then
			        os.rename(PathJoin(InstallDst, modfile) .. ".notverifiedyet", PathJoin(InstallDst, modfile))
					print(string.format("The file %s has a different hash than expected, but it was used from the local original game installation, not downloaded. So ignoring.", modfile))
				else
					os.remove(PathJoin(InstallDst, modfile) .. ".notverifiedyet")
					error(string.format("The file %s was downloaded corrupted. Installation aborted.", modfile))
				end
			else
		        os.rename(PathJoin(InstallDst, modfile) .. ".notverifiedyet", PathJoin(InstallDst, modfile))
				print(string.format("Verified hash of %s", modfile))
			end
		end
	end

	print("Copying miscellaneous files potentially needed by deusex.exe")
	CopyFile(PathJoin(InstallSrc, "system\\detoured.dll"),
		PathJoin(InstallDst, "system\\detoured.dll"))

	print("Removing installation in progress marker file")
	os.remove(PathJoin(InstallDst, "pinstall.txt"))
	print("Installation finished without throwing an error")
	return true
end

finishInstallation = function()
	pprint("Installation finished")
	if ShowFinishDialog(txtGameName .. " - Finished", string.gsub(msgInstallationFinished, "\n", "\r\n"), "&Play Deus Ex Vetery now", true) then
		print("Running Deus Ex Vetery")
		CreateProcess(PathJoin(InstallDst, "System\\DeusEx.exe"), "", InstallDst)
	end
	print("Deus Ex Vetery was successfully installed")
end

handleStartMenu = function()
	pprint("Registering DXVetery")
	dxvpath = PathJoin(GetUserDir("CSIDL_PROGRAMS"), "Deus Ex Vetery")
	print("Creating start menu group")
	CreateDirectory(dxvpath)
	print("Creating start menu item")
	MakeShellLink("Play Deus Ex Vetery", PathJoin(dxvpath, "Deus Ex Vetery.lnk"), PathJoin(PathJoin(InstallDst, "System"), "DeusEx.exe"), PathJoin(InstallDst, "System"))
end

doSetup = function()
	SetWindowTitle(txtGameName .. " Setup")
	pprint("Starting up setup")
	setupSuccess, setupResult = pcall(setupRoutine)
	if setupSuccess then
		if setupResult then
			handleStartMenu()
			print("Installation finished")
			finishInstallation()
			ExitSetup(0)
		else
			ShowTextDialog(txtGameName .. " - Canceled", string.gsub(msgInstallationCanceled, "\n", "\r\n"))
			print("Installation cancelled")
			ExitSetup(2)
		end
	else
		-- MessageBox("Installation error", setupResult, "error");
		ShowTextDialog(txtGameName .. " - Error", string.gsub(string.format(msgError, setupResult), "\n", "\r\n"))
		print("Installation aborted due to error")
		ExitSetup(1);
	end
end
