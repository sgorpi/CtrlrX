#include "stdafx.h"
#include "CtrlrUtilities.h"
#include "CtrlrLog.h"
#include "CtrlrPanel.h"
#include "CtrlrPanel/CtrlrPanelEditor.h"
#include "CtrlrPanel/CtrlrPanelResourceManager.h"
#include "CtrlrPanel/CtrlrPanelResource.h"
#include "CtrlrProcessor.h"
#include "CtrlrMacros.h"
#include "CtrlrManager/CtrlrManager.h"
#include "CtrlrInlineUtilitiesGUI.h"

ValueTree CtrlrPanel::getCleanPanelTree()
{
    ValueTree exportTree = panelTree.createCopy();
    
    for (int i=0; i<exportTree.getNumProperties(); i++)
    {
        exportTree.removeProperty (Ids::panelMidiOutputDevice, 0);
        exportTree.removeProperty (Ids::panelMidiInputDevice, 0);
        exportTree.removeProperty (Ids::panelMidiControllerDevice, 0);
        exportTree.removeProperty (Ids::panelIsDirty, 0); // Added v5.6.31. Removes the panelIsDirty property to prevent crash on load. Problem solved with v5.6.31b getPanelWindowTitle() fixed.
    }
    
    // Remove custom data
    if (exportTree.getChildWithName(Ids::panelCustomData).isValid())
    {
        exportTree.removeChild(exportTree.getChildWithName(Ids::panelCustomData), nullptr);
    }
    
    ValueTree ed = exportTree.getChildWithName(Ids::uiPanelEditor);
    
    if (ed.isValid())
    {
        bool hideMenuBar = (bool)ed.getProperty(Ids::uiPanelMenuBarHideOnExport);
        ed.setProperty(Ids::uiPanelMenuBarVisible, !hideMenuBar, nullptr);
    }
    
    // Embed external lua code in properties
    convertLuaMethodsToPropeties(getPanelLuaDir(),exportTree);
    
    return (exportTree);
}

String CtrlrPanel::getPanelContentDirPath()
{
	const String filePath = getProperty(Ids::panelFilePath);
	return filePath.upToLastOccurrenceOf(".", false, false);
}

File CtrlrPanel::getPanelContentDir()
{
	return File(getPanelContentDirPath());
}

File CtrlrPanel::getPanelLuaDir()
{
	return getPanelContentDir().getChildFile("lua");
}

String CtrlrPanel::getPanelLuaDirPath()
{
	return getPanelLuaDir().getFullPathName();
}

File CtrlrPanel::getPanelResourcesDir()
{
	return getPanelContentDir().getChildFile("resources");
}

String CtrlrPanel::getPanelResourcesDirPath()
{
	return getPanelResourcesDir().getFullPathName();
}

File CtrlrPanel::getLuaMethodGroupDir(const ValueTree &methodGroup)
{
	ValueTree currentItem = methodGroup;
	StringArray temp;
	while (currentItem.isValid() && currentItem.hasType(Ids::luaMethodGroup))
	{
		String currentPath = currentItem.getProperty(Ids::name);
		if (!currentPath.isEmpty())
		{
			temp.add(currentPath);
		}
		currentItem = currentItem.getParent();
	}
	File result(getPanelLuaDirPath());
	for (int i = temp.size(); i >= 0; i--)
	{
		result = result.getChildFile(temp[i]);
	}
	return result;
}

Result CtrlrPanel::convertLuaMethodsToFiles(const String dirPath)
{
	Result res = Result::ok();
	// Try and get access to panel directory
	File panelLuaDirectory(dirPath);
	if (panelLuaDirectory.existsAsFile())
	{	// A directory with that name already exists
		res = Result::fail("Convert to files can't create directory (a file with that name already exists): " + dirPath);
	}
	else if (!panelLuaDirectory.exists())
	{
		res = panelLuaDirectory.createDirectory();
	}
	if (res.ok())
	{
		if (panelLuaDirectory.hasWriteAccess())
		{	// Save lua code
			res = saveLuaCode(panelLuaDirectory, this);
		}
		else
		{
			res = Result::fail("Convert to XML can't write in panel directory: " + dirPath);
		}
	}
	return res;
}

void CtrlrPanel::convertLuaMethodsToPropeties(const File &panelLuaDir, ValueTree &panelTree)
{
	ValueTree luaManager = panelTree.getChildWithName(Ids::luaManager);
	if (luaManager.isValid())
	{
		ValueTree luaMethods = luaManager.getChildWithName(Ids::luaManagerMethods);
		CtrlrPanel::convertLuaChildrenToProperties(panelLuaDir, &luaMethods);
	}
}

// Updated v8.0.100. The result goes to a callback. A panel that was never saved asks for its file
// with an asynchronous chooser and is saved in its callback, so a close waits for the answer. A
// panel with a file is saved at once and the callback runs inside the call.
void CtrlrPanel::savePanel (std::function<void (const Result &)> callback)
{
    _DBG("CtrlrPanel::savePanel");
    
    // Store current state panelWasDirty before changing the property panelIsDirty to false/0
    bool panelWasDirty = isPanelDirty(); // return getProperty(Ids::panelIsDirty,false); with false (0) being the defaultReturnValue
    setPanelDirty(false);
    
    const String filePath = getProperty(Ids::panelFilePath);
    File panelFile(filePath);

    // The end of both branches.
    const auto finish = [this, panelWasDirty, callback] (const Result &res)
    {
        if (res.wasOk())
        {
            getUndoManager()->clearUndoHistory();
            updatePanelWindowTitle();
        }
        
        else if (panelWasDirty)
        {
            setPanelDirty(panelWasDirty); // setProperty(Ids::panelIsDirty, dirty);
        }

        if (callback)
            callback (res);
    };
    
    if (panelFile.existsAsFile() && panelFile.hasWriteAccess())
    {
        Result res = Result::ok();
        if (panelFile.hasFileExtension("panel"))
            res = savePanelXml(panelFile, this, false);
        if (panelFile.hasFileExtension("panelz"))
            res = savePanelXml (panelFile, this, true);
        if (panelFile.hasFileExtension("bpanel"))
            res = savePanelBin (panelFile, this, false);
        if (panelFile.hasFileExtension("bpanelz"))
            res = savePanelBin(panelFile, this, true);
        
        if (getEditor())
        {
            if (res.failed())
            {
                notify ("Panel save: ["+res.getErrorMessage()+"]", nullptr, NotifyFailure);
            }
            else
            {
                notify ("Panel saved: ["+panelFile.getFullPathName()+"]", nullptr, NotifySuccess);
            }
        }
        finish (res);
    }
    else
    {
        // The callback does nothing if the panel was closed while the chooser was open.
        WeakReference<CtrlrPanel> safeThis (this);
        askForPanelFileToSaveAsync (this, File(owner.getProperty(Ids::panelLastSaveDir)).getChildFile(getVersionString()), true, false,
                                    [this, safeThis, panelFile, finish] (const File &ret)
        {
            if (safeThis == nullptr)
                return;

            Result res = Result::ok();
            if (ret != File())
            {
                res = savePanelXml(ret, this);
                setProperty (Ids::panelFilePath, ret.getFullPathName());
                setProperty (Ids::panelLastSaveDir, ret.getParentDirectory().getFullPathName());
            }
            else
            {
                res = Result::fail ("Selected file is invalid");
            }
            
            if (getEditor())
            {
                if (res.failed())
                {
                    notify ("Panel save: ["+res.getErrorMessage()+"]", nullptr, NotifyFailure);
                }
                else
                {
                    notify ("Panel saved: ["+panelFile.getFullPathName()+"]", nullptr, NotifySuccess);
                }
            }
            finish (res);
        });
    }
}

void CtrlrPanel::savePanelAs(const CommandID saveOption)
{
    File f(getProperty(Ids::panelLastSaveDir));
    // Added v8.0.100. The file choosers are asynchronous: each format is saved in the chooser's
    // callback, which does nothing if the panel was closed while the chooser was open.
    WeakReference<CtrlrPanel> safeThis (this);
    
    if (saveOption == CtrlrEditor::doExportFileText)
    {
        askForPanelFileToSaveAsync (this, f, true, false, [this, safeThis] (const File &fileToSave)
        {
            if (safeThis == nullptr || fileToSave == File())
                return;
        
            savePanelXml (fileToSave, this);
            setProperty (Ids::panelFilePath, fileToSave.getFullPathName());
            setProperty (Ids::panelLastSaveDir, fileToSave.getParentDirectory().getFullPathName());
        
            // Store current state panelWasDirty before changing the property panelIsDirty to false/0
            bool panelWasDirty = isPanelDirty(); // Added v5.6.30 (removes asterisk suffix from name in panel tab). Returns getProperty(Ids::panelIsDirty,false); where false is default value if not available
        
            setPanelDirty(false); // Updated v5.6.31. false = 0 = notDirty.
        
            if (panelWasDirty) // Added v5.6.30. if panelPanelWasDirty = true/1
            {
                setPanelDirty(panelWasDirty); // Added v5.6.30. setProperty(Ids::panelIsDirty, dirty);
            }
        
            getUndoManager()->clearUndoHistory(); // Added v5.6.30
            updatePanelWindowTitle(); // Added v5.6.30
        });
    }
    
    
	if (saveOption == CtrlrEditor::doExportFileZText)
	{
		askForPanelFileToSaveAsync (this, f, true, true, [this, safeThis] (const File &fileToSave)
		{
			if (safeThis == nullptr || fileToSave == File())
				return;

			savePanelXml (fileToSave, this, true);
			setProperty (Ids::panelFilePath, fileToSave.getFullPathName());
			setProperty (Ids::panelLastSaveDir, fileToSave.getParentDirectory().getFullPathName());
		});
	}
	if (saveOption == CtrlrEditor::doExportFileBin)
	{
		askForPanelFileToSaveAsync (this, f, false, false, [this, safeThis] (const File &fileToSave)
		{
			if (safeThis == nullptr || fileToSave == File())
				return;

			savePanelBin (fileToSave, this, false);
		});
	}
	if (saveOption == CtrlrEditor::doExportFileZBin)
	{
		askForPanelFileToSaveAsync (this, f, false, true, [this, safeThis] (const File &fileToSave)
		{
			if (safeThis == nullptr || fileToSave == File())
				return;

			savePanelBin (fileToSave, this, true);
		});
	}
	if (saveOption == CtrlrEditor::doExportFileZBinRes)
	{
		// Asks here and exports in the callback. A cancelled chooser gets exportPanelAsync()'s message.
		askForPanelFileToSaveAsync (this, f, false, true, [this, safeThis, f] (const File &exportedFile)
		{
			if (safeThis == nullptr)
				return;

			// Nothing to show when the panel was closed while a licence question was open.
			const auto showError = [safeThis] (const String &err, const MemoryBlock &, const MemoryBlock &)
			{
				if (safeThis != nullptr && err != "")
				{
					AW::showMessageBox(AlertWindow::WarningIcon, "Panel Export", err);
				}
			};
			if (exportedFile == File())
				showError ("Destination file does not exist or is invalid", MemoryBlock(), MemoryBlock());
			else
				exportPanelAsync(this, f, exportedFile, false, false, showError);
		});
	}
	if (saveOption == CtrlrEditor::doExportFileInstance) // Updated v5.6.34.
	{
		owner.getNativeObject().exportWithDefaultPanel(this, false, false, [this, safeThis] (Result res)
		{
			if (safeThis == nullptr)
				return;

			if (res.failed())
			{
				if (res.getErrorMessage() == "User cancelled the export operation.")
				{
					// Silently handle the cancellation. No message box.
					notify("Panel instance export: Cancelled by user.", nullptr, NotifyFailure);
				}
				else
				{
					notify("Panel instance export: [" + res.getErrorMessage() + "]", nullptr, NotifyFailure);
					// Handle a true export failure with a warning message.
					AW::showMessageBox(AlertWindow::WarningIcon, "Panel export", "Failed to export panel as standalone instance.\n" + res.getErrorMessage());
				}
			}
			else
			{
				notify("Panel instance export: Wrote new panel instance.", nullptr, NotifySuccess);
				// Handle a successful export.
				AW::showMessageBox(AlertWindow::InfoIcon, "Panel export", "Wrote new panel instance");
			}
		});
	}

	if (saveOption == CtrlrEditor::doExportFileInstanceRestricted) // Updated v5.6.34.
	{
		owner.getNativeObject().exportWithDefaultPanel(this, true, true, [this, safeThis] (Result res)
		{
			if (safeThis == nullptr)
				return;

			if (res.failed())
			{
				if (res.getErrorMessage() == "User cancelled the export operation.")
				{
					// Silently handle the cancellation. No message box.
					notify("Panel instance export: Cancelled by user.", nullptr, NotifyFailure);
				}
				else
				{
					notify("Panel instance export: [" + res.getErrorMessage() + "]", nullptr, NotifyFailure);
					// Handle a true export failure with a warning message.
					AW::showMessageBox(AlertWindow::WarningIcon, "Panel export", "Failed to export panel as standalone instance.\n" + res.getErrorMessage());
				}
			}
			else
			{
				notify("Panel instance export: Wrote new panel instance.", nullptr, NotifySuccess);
				// Handle a successful export.
				AW::showMessageBox(AlertWindow::InfoIcon, "Panel export", "Wrote new panel instance");
			}
		});
	}
	if (saveOption == CtrlrEditor::doExportGenerateUID)
	{
		setProperty (Ids::panelUID, generateRandomUnique(STR (Time::currentTimeMillis())));
	}
}

void CtrlrPanel::savePanelVersioned()
{
	File panelFile(getProperty (Ids::panelFilePath));

	if (panelFile.existsAsFile() && panelFile.hasWriteAccess())
	{
		setProperty (Ids::panelVersionMinor, (int)getProperty(Ids::panelVersionMinor)+1);

		if (panelFile != File())
		{
			savePanelXml (File(panelFile.getParentDirectory().getChildFile(getProperty(Ids::name).toString() + owner.getProperty(Ids::ctrlrVersionSeparator).toString() + getVersionString()).withFileExtension((bool)owner.getProperty(Ids::ctrlrVersionCompressed) ? "panelz" : "panel")), this, owner.getProperty(Ids::ctrlrVersionCompressed));
		}
	}
	else
	{
		savePanel();
	}
}

namespace
{
	// The licence window of an export. The text box lives as long as the window.
	struct LicenseContentWindow : public AlertWindow
	{
		LicenseContentWindow() : AlertWindow ("License content", "Paste your license below", AlertWindow::InfoIcon, 0)
		{
			lic.setMultiLine (true, false);
			lic.setSize (400,300);
			addCustomComponent (&lic);
			addButton ("OK", 1);
		}

		TextEditor lic;
	};

	// The part of CtrlrPanel::exportPanelAsync() after the licence questions.
	void finishPanelExport (WeakReference<CtrlrPanel> panelRef, ValueTree resources, const File &exportedFile,
	                        const Image &panelSnapshot, const bool toMemory, const bool isRestricted,
	                        const CtrlrPanel::ExportCallback &callback)
	{
		const auto done = [&callback] (const String &error, const MemoryBlock &outputPanelData, const MemoryBlock &outputResourcesData)
		{
			if (callback)
				callback (error, outputPanelData, outputResourcesData);
		};

		// The panel may have been closed while a licence question was open.
		CtrlrPanel *panel = panelRef.get();
		if (panel == nullptr)
		{
			done ("The panel was closed during the export", MemoryBlock(), MemoryBlock());
			return;
		}

		ValueTree exportTree	= panel->getCleanPanelTree();

		if (isRestricted)
		{
			exportTree.setProperty (Ids::restricted, (int)InstanceSingleRestricted, nullptr);
			if (exportTree.getChildWithName (Ids::uiPanelEditor).isValid())
			{
				exportTree.getChildWithName (Ids::uiPanelEditor).setProperty (Ids::uiPanelEditMode, false, nullptr);
			}
		}

		if (panelSnapshot != Image())
		{
			MemoryBlock imageData;
			MemoryOutputStream imageDataStream(imageData, true);
			PNGImageFormat png;
			png.writeImageToStream (panelSnapshot, imageDataStream);

			if (imageData.getSize() != 0)
			{
				ValueTree snap(Ids::resourcePanelSnapshot);
				snap.setProperty (Ids::resourceSize, (int)imageData.getSize(), 0);
				snap.setProperty (Ids::resourceData, imageData.toBase64Encoding(), 0);
				resources.addChild (snap, -1, 0);
			}
		}

		if (exportedFile.hasWriteAccess() && !toMemory)
		{
			exportTree.addChild (resources, -1, 0);
			MemoryOutputStream compressedData;

			{
				GZIPCompressorOutputStream gzipOutputStream (&compressedData);
				exportTree.writeToStream(gzipOutputStream);
				gzipOutputStream.flush();
			}

			exportedFile.replaceWithData(compressedData.getData(), compressedData.getDataSize());

			done ("", MemoryBlock(), MemoryBlock());
		}

		else if (toMemory)
		{
			MemoryBlock outputPanelData, outputResourcesData;

			{
				MemoryOutputStream compressedPanelData(outputPanelData, false);
				MemoryOutputStream compressedResourcesData(outputResourcesData, false);

				{
					GZIPCompressorOutputStream gzipOutputStream (&compressedPanelData);
					exportTree.writeToStream(gzipOutputStream); // exportTree
					gzipOutputStream.flush();
				}

				{
					GZIPCompressorOutputStream gzipOutputStream (&compressedResourcesData);
					resources.writeToStream(gzipOutputStream);
					gzipOutputStream.flush();
				}
			} // the streams set the blocks' final sizes when they are deleted

			done ("", outputPanelData, outputResourcesData);
		}
		else
		{
			done ("Can't export panel, I can't write to the specified file", MemoryBlock(), MemoryBlock());
		}
	}
}

// Updated v8.0.100. Asynchronous: the licence question and the licence window are asynchronous
// alerts, and the export goes on in their callbacks. The callback gets the error ("" when it
// worked) and, with toMemory, the panel and resources data for an exported instance (the
// MemoryBlock pointers of the blocking version).
void CtrlrPanel::exportPanelAsync(CtrlrPanel *panel, const File &lastBrowsedDir, const File &destinationFile, const bool toMemory, const bool isRestricted, ExportCallback callback)
{
	const auto fail = [&callback] (const String &error)
	{
		if (callback)
			callback (error, MemoryBlock(), MemoryBlock());
	};

	Image panelSnapshot (Image::ARGB, 400, 400, true);
	CtrlrPanelCanvas *canvas = panel->getEditor()->getCanvas();
	// CtrlrPanelEditor *editor = panel->getEditor();

	if (panel == 0 || panel == nullptr)
		return (fail ("Undefined panel passeed to exporter"));

	File exportedFile = File();

	if (destinationFile == File())
		exportedFile = askForPanelFileToSave (panel, lastBrowsedDir, false, true);
	else
		exportedFile = destinationFile;

	if (exportedFile == File())
		return (fail ("Destination file does not exist or is invalid"));

	panel->luaSavePanel (PanelFileExport, exportedFile);

	if (canvas)
	{
		Image snap	= canvas->createComponentSnapshot (canvas->getBounds(), true);
		Graphics g(panelSnapshot);
		g.drawImageWithin (snap, 0, 0, 400, 400, RectanglePlacement::centred | RectanglePlacement::onlyReduceInSize, false);
	}

	ValueTree resources(Ids::resourceExportList);

	for (int i=0; i<panel->getResourceManager().getNumResources(); i++)
	{
		CtrlrPanelResource *res = panel->getResourceManager().getResource(i);
		if (res)
		{
			resources.addChild (res->createTree(), -1, 0);
		}
	}

	WeakReference<CtrlrPanel> panelRef (panel);

	if (panel->getResourceManager().getNumResources() > 0)
	{
		AW::showOkCancelBox (AlertWindow::QuestionIcon, "License question", "Would you like to attach a license to this panel?", "Yes", "No", nullptr,
		                     [=] (bool attachLicense)
		{
			if (!attachLicense)
				return (finishPanelExport (panelRef, resources, exportedFile, panelSnapshot, toMemory, isRestricted, callback));

			AW::runModalAsync<LicenseContentWindow> (std::make_unique<LicenseContentWindow>(),
			                                         [=] (int result, LicenseContentWindow &licenseWindow)
			{
				if (result)
				{
					ValueTree licTree(Ids::resourceLicense);
					licTree.setProperty(Ids::resourceData, licenseWindow.lic.getText(), 0);
					ValueTree (resources).addChild (licTree, -1, 0);
				}
				finishPanelExport (panelRef, resources, exportedFile, panelSnapshot, toMemory, isRestricted, callback);
			});
		});
		return;
	}

	finishPanelExport (panelRef, resources, exportedFile, panelSnapshot, toMemory, isRestricted, callback);
}

const ValueTree CtrlrPanel::openBinPanel(const File &panelFile)
{
	ValueTree tree;

	if (panelFile.hasFileExtension(".bpanelz"))
	{
		std::unique_ptr<FileInputStream> fileInputStream(panelFile.createInputStream());

		if (fileInputStream)
		{
            GZIPDecompressorInputStream gzFileInputStream (*fileInputStream);
			return (ValueTree::readFromStream (gzFileInputStream));
		}
	}
	else if (panelFile.hasFileExtension(".bpanel"))
	{
		std::unique_ptr<FileInputStream> fileInputStream(panelFile.createInputStream());
		if (fileInputStream)
		{
			return (ValueTree::readFromStream (*fileInputStream));
		}
	}

	return (ValueTree());
}

const ValueTree CtrlrPanel::openBinPanel(const MemoryBlock &panelData, const bool isCompressed)
{
	ValueTree tree;

	if (isCompressed)
	{
		MemoryInputStream mi(panelData, false);

		{
			GZIPDecompressorInputStream gzFileInputStream (mi);
			tree = ValueTree::readFromStream (gzFileInputStream);

			return (tree);
		}
	}
	else
	{
		{
			MemoryInputStream mi(panelData, false);
			return (ValueTree::readFromStream (mi));
		}
	}
}

const ValueTree CtrlrPanel::openXmlPanel(const File &panelFile)
{
	String xmlData;

	if (panelFile.hasFileExtension("panelz"))
	{
		std::unique_ptr<FileInputStream> fz(panelFile.createInputStream());
		if (fz)
		{
			GZIPDecompressorInputStream gzInput (fz.get(), false);
			xmlData = gzInput.readEntireStreamAsString();
		}
		else
		{
			_ERR("CtrlrPanel::openXmlPanel can't create input stream for file: " + panelFile.getFullPathName());
		}

		std::unique_ptr<XmlElement> xml(XmlDocument::parse(xmlData));

		if (xml)
		{
			return (ValueTree::fromXml (*xml));
		}
		else
		{
			_ERR("CtrlrPanel::openXmlPanel can't parse file contents as XML");
		}
	}
	else if (panelFile.hasFileExtension("panel"))
	{
		std::unique_ptr<XmlElement> xml(XmlDocument::parse(panelFile));
		if (xml)
		{
			return (ValueTree::fromXml (*xml));
		}
		else
		{
			_ERR("CtrlrPanel::openXmlPanel can't parse file contents as XML");
		}
	}
	else
	{
		_ERR("CtrlrPanel::openXmlPanel unknown file type");
	}

	_ERR("CtrlrPanel::openXmlPanel can't open panel");
	return (ValueTree());
}

const ValueTree CtrlrPanel::openPanel(const File &panelFile)
{
	ValueTree result;
	if (panelFile.hasFileExtension("panelz;panel"))
	{
		result = openXmlPanel(panelFile);
	}
	else if (panelFile.hasFileExtension("bpanelz;bpanel"))
	{
		result = openBinPanel(panelFile);
	}
	else
	{
		result = ValueTree();
	}
	// Patch panelFilePath property to match the actual file
	result.setProperty(Ids::panelFilePath,panelFile.getFullPathName(),nullptr);
	return result;
}

Result CtrlrPanel::savePanelBin(const File &fileToSave, CtrlrPanel *panel, const bool compressPanel)
{
	MemoryOutputStream panelBinData;

	if (panel == nullptr)
		return (Result::fail ("Invalid panel pointer"));

	panel->sync();

	if (compressPanel)
	{
		panel->luaSavePanel (PanelFileBinaryCompressed, fileToSave);

		GZIPCompressorOutputStream gzOutputStream (&panelBinData);
		panel->getPanelTree().writeToStream(gzOutputStream);
		gzOutputStream.flush();
	}
	else
	{
		panel->luaSavePanel (PanelFileBinary, fileToSave);
		panel->getPanelTree().writeToStream(panelBinData);
	}

	if (fileToSave.hasWriteAccess())
	{
		if (fileToSave.replaceWithData(panelBinData.getData(), panelBinData.getDataSize()))
		{
			return (Result::ok());
		}
		else
		{
			return (Result::fail ("savePanelBin replaceWithData() failed on destination file "+fileToSave.getFullPathName()));
		}
	}
	else
	{
		AW::showMessageBox(AlertWindow::WarningIcon, "Can't save panel", "I can't write to the specified file", "OK");
		return (Result::fail ("savePanelBin now write access to file: "+fileToSave.getFullPathName()));
	}
}

void CtrlrPanel::writePanelXml(OutputStream &outputStream, CtrlrPanel *panel, const bool compressPanel)
{
	if (panel == nullptr)
		return;

	panel->sync();

	std::unique_ptr<XmlElement> panelXml(panel->getPanelTree().createXml());

	if (compressPanel && panelXml)
	{
		String xml = panelXml->createDocument("");
		{
			GZIPCompressorOutputStream gzipOutputStream(&outputStream, 9, false);
			gzipOutputStream.writeString (xml);
		}
	}
	if (!compressPanel && panelXml)
	{
		panelXml->writeToStream (outputStream,"");
	}
}

File CtrlrPanel::getLuaMethodSourceFile(const ValueTree *method)
{
	String path = method->getProperty(Ids::luaMethodSourcePath);
	if (File::isAbsolutePath(path))
	{
		return File(path);
	}
	else
	{
		return getPanelLuaDir().getChildFile(path);
	}
}


Result CtrlrPanel::writeLuaMethod(const File &parentDir, ValueTree *method)
{
	if (method == nullptr)
		return Result::fail("Method name is missing");

	const String methodName = method->getProperty(Ids::luaMethodName);
	const String methodCode = method->getProperty(Ids::luaMethodCode);
	if (methodName.isEmpty())
		return Result::fail("Method name is empty");

	// Create file
	const File methodFile = parentDir.getNonexistentChildFile(methodName,".lua",false);
	if (methodFile.replaceWithText(methodCode))
	{
		method->setProperty(Ids::luaMethodName, methodFile.getFileNameWithoutExtension(), nullptr);
		method->setProperty(Ids::luaMethodSourcePath, methodFile.getRelativePathFrom(getPanelLuaDir()), nullptr);
		method->setProperty(Ids::luaMethodSource, (int)CtrlrLuaMethod::codeInFile, nullptr);
		method->removeProperty(Ids::luaMethodCode,nullptr);
		return Result::ok();
	}
	else
	{
		AW::showMessageBox(AlertWindow::WarningIcon, "Could not save lua method", "Could not save lua method", "OK");
		return (Result::fail("saveLuaMethod: could not save lua method."));
	}
}

Result CtrlrPanel::writeLuaMethodGroup(const File &parentDir, ValueTree *methodGroup)
{
	if (methodGroup == nullptr)
		return Result::fail("Method group is missing");

	const String methodGroupName = methodGroup->getProperty(Ids::name);
	if (methodGroupName.isEmpty())
		return Result::fail("Method group is empty");

	// Create file
	const File methodGroupFile = parentDir.getChildFile(methodGroupName);
	Result res = methodGroupFile.createDirectory();
	if (res.wasOk())
	{	// Save contained methods and groups
		return CtrlrPanel::writeLuaChildren(methodGroupFile, methodGroup);
	}
	else
	{
		AW::showMessageBox(AlertWindow::WarningIcon, "Could not save lua method group", "Could not save lua method group", "OK");
	}
	return res;
}

Result CtrlrPanel::writeLuaChildren(const File &parentDir, ValueTree *parentElement)
{
	if (parentElement == nullptr)
		return Result::fail("Parent element is missing");

	Result res = Result::ok();
	for (int i = 0; i<parentElement->getNumChildren(); i++)
	{
		ValueTree child = parentElement->getChild(i);
		if (child.hasType(Ids::luaMethod))
		{
			if ((int)child.getProperty(Ids::luaMethodSource) != CtrlrLuaMethod::codeInFile)
			{	// Only process methods that are not already saved in files
				res = CtrlrPanel::writeLuaMethod(parentDir, &child);
				if (res.failed())
				{	// Break on first error
					return res;
				}
			}
		}
		else if (child.hasType(Ids::luaMethodGroup))
		{
			res = CtrlrPanel::writeLuaMethodGroup(parentDir, &child);
			if (res.failed())
			{	// Break on first error
				return res;
			}
		}
	}
	return res;
}

Result CtrlrPanel::saveLuaCode(const File &panelLuaDir, CtrlrPanel *panel)
{
	ValueTree luaManager = panel->getPanelTree().getChildWithName(Ids::luaManager);
	if (luaManager.isValid())
	{
		ValueTree luaMethods = luaManager.getChildWithName(Ids::luaManagerMethods);
		return CtrlrPanel::writeLuaChildren(panelLuaDir,&luaMethods);
	}
	else
	{
		return (Result::fail("saveLuaCode failed due to missing luaManager"));
	}
}

void CtrlrPanel::convertLuaMethodToProperty(const File &panelLuaDir, ValueTree *method)
{
	if (method == nullptr)
		return;

	const String methodName = method->getProperty(Ids::luaMethodName);
	const String methodFilePath = method->getProperty(Ids::luaMethodSourcePath);
	if (methodName.isEmpty() || methodFilePath.isEmpty())
		return;
	// Get file path
	File methodFile;
	if (File::isAbsolutePath(methodFilePath))
	{
		methodFile = File(methodFilePath);
	}
	else
	{
		methodFile = panelLuaDir.getChildFile(methodFilePath);
	}

	// Read file
	method->removeProperty(Ids::luaMethodSourcePath,nullptr);
	method->setProperty(Ids::luaMethodSource, (int)CtrlrLuaMethod::codeInProperty, nullptr);
	method->setProperty(Ids::luaMethodCode,methodFile.loadFileAsString(), nullptr);
}

void CtrlrPanel::convertLuaChildrenToProperties(const File &panelLuaDir, ValueTree *parentElement)
{
	if (parentElement == nullptr)
		return;

	for (int i = 0; i<parentElement->getNumChildren(); i++)
	{
		ValueTree child = parentElement->getChild(i);
		if (child.hasType(Ids::luaMethod))
		{	// This is a method, check if it's on file
			if ((int)child.getProperty(Ids::luaMethodSource) == CtrlrLuaMethod::codeInFile)
			{	// Only process methods that are saved in files
				CtrlrPanel::convertLuaMethodToProperty(panelLuaDir, &child);
			}
		}
		else if (child.hasType(Ids::luaMethodGroup))
		{	// This is a group => recursive call
			CtrlrPanel::convertLuaChildrenToProperties(panelLuaDir, &child);
		}
	}
}

Result CtrlrPanel::savePanelXml(const File &fileToSave, CtrlrPanel *panel, const bool compressPanel)
{
	MemoryOutputStream dataToSave;

	if (compressPanel)
	{
		panel->luaSavePanel (PanelFileXMLCompressed, fileToSave);
	}
	else
	{
		panel->luaSavePanel (PanelFileXML, fileToSave);
	}

	writePanelXml (dataToSave, panel, compressPanel);

	if (fileToSave.hasWriteAccess())
	{
		if (fileToSave.replaceWithData(dataToSave.getData(), dataToSave.getDataSize()))
		{
			return (Result::ok());
		}
		else
		{
			return (Result::fail ("savePanelXml replaceWithData() failed on destination file "+fileToSave.getFullPathName()));
		}
	}
	else
	{
		AW::showMessageBox(AlertWindow::WarningIcon, "Can't save panel", "I can't write to the specified file", "OK");
		return (Result::fail ("savePanelXml now write access to file: "+fileToSave.getFullPathName()));
	}
}

// The file that the panel save chooser starts with: the panel's name and version
// with the format's extension, in lastBrowsedDir or else in the documents folder.
const File CtrlrPanel::defaultPanelFileToSave (CtrlrPanel *panel,
												const File &lastBrowsedDir,
												const bool isXml,
												const bool isCompressed)
{
	String panelFileName = "Ctrlr Panel";
	File panelFile;
	if (panel)
	{
		panelFileName = panel->getProperty(Ids::name);
		panelFileName << "_" + panel->getVersionString();
	}

	if (isXml)
	{
		if (isCompressed)
			panelFileName << ".panelz";
		else
			panelFileName << ".panel";
	}
	else
	{
		if (isCompressed)
			panelFileName << ".bpanelz";
		else
			panelFileName << ".bpanel";
	}

	if (File::isAbsolutePath(lastBrowsedDir.getFullPathName()))
	{
		panelFile = lastBrowsedDir.getChildFile(panelFileName);
	}
	else
	{
		panelFile = File::getSpecialLocation(File::userDocumentsDirectory).getChildFile(panelFileName);
	}

	return (panelFile);
}

const File CtrlrPanel::askForPanelFileToSave (CtrlrPanel *panel,
												const File &lastBrowsedDir,
												const bool isXml,
												const bool isCompressed)
{
	File panelFile = defaultPanelFileToSave (panel, lastBrowsedDir, isXml, isCompressed);
	bool useOSDialog = panel ? (bool)panel->getCtrlrManagerOwner().getProperty(Ids::ctrlrNativeFileDialogs) : true;

	if (isXml)
	{
		if (isCompressed)
		{
			FileChooser fileChooser ("Panel file XML compressed", panelFile, "*.panelz", useOSDialog);
			if (fileChooser.browseForFileToSave (true))
			{
				return (fileChooser.getResult().withFileExtension("panelz"));
			}
			else
			{
				if (panel)
					panel->notify ("Save file dialog failed", nullptr, NotifyFailure);
			}
		}
		else
		{
			FileChooser fileChooser ("Panel file XML", panelFile, "*.panel", useOSDialog);
			if (fileChooser.browseForFileToSave (true))
			{
				return (fileChooser.getResult().withFileExtension("panel"));
			}
			else
			{
				if (panel)
					panel->notify ("Save file dialog failed", nullptr, NotifyFailure);
			}
		}
	}
	else
	{
		if (isCompressed)
		{
			FileChooser fileChooser ("Panel file binary compressed", panelFile, "*.bpanelz", useOSDialog);
			if (fileChooser.browseForFileToSave (true))
			{
				return (fileChooser.getResult().withFileExtension("bpanelz"));
			}
			else
			{
				if (panel)
					panel->notify ("Save file dialog failed", nullptr, NotifyFailure);
			}
		}
		else
		{
			FileChooser fileChooser ("Panel file binary", panelFile, "*.bpanel", useOSDialog);
			if (fileChooser.browseForFileToSave (true))
			{
				return (fileChooser.getResult().withFileExtension("bpanel"));
			}
			else
			{
				if (panel)
					panel->notify ("Save file dialog failed", nullptr, NotifyFailure);
			}
		}
	}

	return (File());
}

// askForPanelFileToSave() with an asynchronous chooser (same title, pattern,
// native flag and failure notice). The callback gets the chosen file with the format's extension,
// or File() for Cancel.
void CtrlrPanel::askForPanelFileToSaveAsync (CtrlrPanel *panel,
												const File &lastBrowsedDir,
												const bool isXml,
												const bool isCompressed,
												std::function<void (const File &)> callback)
{
	const File panelFile = defaultPanelFileToSave (panel, lastBrowsedDir, isXml, isCompressed);
	const bool useOSDialog = panel ? (bool)panel->getCtrlrManagerOwner().getProperty(Ids::ctrlrNativeFileDialogs) : true;
	const String extension = isXml ? (isCompressed ? "panelz" : "panel") : (isCompressed ? "bpanelz" : "bpanel");
	const String title = isXml ? (isCompressed ? "Panel file XML compressed" : "Panel file XML")
	                           : (isCompressed ? "Panel file binary compressed" : "Panel file binary");
	WeakReference<CtrlrPanel> safePanel (panel);

	FC::saveFileAsync (title, panelFile, "*." + extension, useOSDialog, [safePanel, extension, callback] (const File &result)
	{
		if (result == File())
		{
			if (safePanel != nullptr)
				safePanel->notify ("Save file dialog failed", nullptr, NotifyFailure);
			callback (File());
			return;
		}
		callback (result.withFileExtension (extension));
	});
}

bool CtrlrPanel::isPanelFile(const File &fileToCheck, const bool beThorough)
{
	if (beThorough)
	{
		return (false);
	}
	else
	{
		if (fileToCheck.hasFileExtension("bpanel;bpanelz;panel;panelz"))
		{
			return (true);
		}
		else
		{
			return (false);
		}
	}
}

void CtrlrPanel::setSavePoint()
{
	indexOfSavedState = currentActionIndex;
}

bool CtrlrPanel::hasChangedSinceSavePoint()
{
	return currentActionIndex != indexOfSavedState;
}

bool CtrlrPanel::isPanelDirty()
{
	return getProperty(Ids::panelIsDirty, false);
}

void CtrlrPanel::setPanelDirty(const bool dirty)
{
	setProperty(Ids::panelIsDirty, dirty);
}

void CtrlrPanel::actionPerformed()
{
	currentActionIndex++;
	updatePanelWindowTitle();
}

void CtrlrPanel::actionUndone()
{
	currentActionIndex--;
	updatePanelWindowTitle();
}

const String CtrlrPanel::getPanelWindowTitle()
{
    String name = getProperty(Ids::name);
    if (JUCEApplication::isStandaloneApp()) // Updated v5.6.31b. For Standalone APP/EXE Only. Was crashing VST Hosts on load in v5.6.30 & v5.6.31
    {
        if (isPanelDirty() || hasChangedSinceSavePoint())
        {
            name = name + "*";
        }
        
        else if(!JUCEApplication::isStandaloneApp()) // Updated v5.6.31b. For VST & AU Plugins
            
            if (isPanelDirty()) // Updated v5.6.31b. Was (isPanelDirty() || hasChangedSinceSavePoint()). Was crashing VST Hosts on load in v5.6.30 & v5.6.31
            {
                name = name + "*";
            }
    }
    
    return name;
}

void CtrlrPanel::updatePanelWindowTitle()
{
	CtrlrPanelEditor *editor = getEditor(false);
	if (editor)
	{
		String newName = getPanelWindowTitle();
		if (newName != editor->getName())
		{
			editor->setName(newName);
			// Trigger editor window title update
			owner.getEditor()->activeCtrlrChanged();
		}
	}
}

void CtrlrPanel::luaManagerChanged()
{
	if (!getRestoreState())
	{
		setPanelDirty(true);
		updatePanelWindowTitle();
	}
}

void CtrlrPanel::panelResourcesChanged()
{
	if (!getRestoreState())
	{
		setPanelDirty(true);
		updatePanelWindowTitle();
	}
}


// Updated v8.0.100. Asynchronous: the callback gets the answer (true = close). The questions and
// their order are as before: the Lua editor's unsaved code first, then the panel's changes (only
// when the panel itself closes). Save waits for the save, also for its chooser; the panel then
// closes whatever the save gave, as before. Nothing more is asked if the panel is gone meanwhile.
void CtrlrPanel::canClose (const bool closePanel, std::function<void (bool)> callback)
{
	WeakReference<CtrlrPanel> safeThis (this);

	// Check for panel modifications
	const auto askAboutPanel = [this, safeThis, closePanel, callback] (bool result)
	{
		if (safeThis == nullptr)
		{
			callback (false);
			return;
		}

		if(closePanel && (hasChangedSinceSavePoint() || isPanelDirty()))
		{
			AW::showYesNoCancelBox(AlertWindow::QuestionIcon, "Save panel (" + getName() + ")", "There are unsaved changes in this panel.\nDo you want to save them before closing ?", "Save", "Discard", "Cancel", nullptr, // Updated v5.6.31 by GoodWeather
			                       [this, safeThis, callback] (int ret)
			{
				if (safeThis == nullptr)
				{
					callback (false);
				}
				else if (ret == 0)
				{	// Cancel
					callback (false);
				}
				else if (ret == 1)
				{	// Save
					savePanel ([callback] (const Result &) { callback (true); });
				}
				else
				{	// Discard
					callback (true);
				}
			});
			return;
		}
		callback (result);
	};

	// Check for modified Lua Code
	CtrlrPanelWindowManager &manager = getWindowManager();
	if (manager.isCreated(CtrlrPanelWindowManager::LuaMethodEditor))
	{
		CtrlrChildWindowContent *content = manager.getContent(CtrlrPanelWindowManager::LuaMethodEditor);
		if (content != nullptr)
		{	// Move the editor to front
			content->toFront(true);
			content->canCloseWindow (askAboutPanel);
			return;
		}
	}
	askAboutPanel (true);
}
