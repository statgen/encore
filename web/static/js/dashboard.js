

function hideUploadOverlay()
{
    document.getElementById("upload_overlay").style.display = "none";
    document.getElementsByName("ped_upload_form")[0].reset();
    document.getElementsByName("ped_file_upload_progress")[0].value = 0;
}

function fileSelected()
{
    var file = document.getElementsByName("ped_file")[0].files[0];
    if (file)
    {
        var fileSize = 0;
        if (file.size > 1024 * 1024)
            fileSize = (Math.round(file.size * 100 / (1024 * 1024)) / 100).toString() + "MB";
        else
            fileSize = (Math.round(file.size * 100 / 1024) / 100).toString() + "KB";

        document.getElementsByName("ped_filename")[0].value = file.name;
        //document.getElementById("fileSize").innerHTML = "Size: " + fileSize;
        //document.getElementById("fileType").innerHTML = "Type: " + file.type;
    }
    else
    {
        document.getElementsByName("ped_filename")[0].value = "";
    }
}

function uploadProgress(evt)
{
    if (evt.lengthComputable)
    {
        var percentComplete = Math.round(evt.loaded * 100 / evt.total);
        document.getElementsByName("ped_file_upload_progress")[0].value = percentComplete.toString();
    }
    else
    {
        //document.getElementById("progressNumber").innerHTML = "unable to compute";
    }
}

function uploadComplete(evt)
{
    /* This event is raised when the server send back a response */
    alert(evt.target.responseText);
}

function uploadFailed(evt)
{
    alert("There was an error attempting to upload the file.");
}

function uploadCanceled(evt)
{
    alert("The upload has been canceled by the user or the browser dropped the connection.");
}

function uploadFile()
{
    var job_name = document.getElementsByName("job_name")[0].value;
    if (document.getElementsByName("ped_file")[0].files.length !== 1 || !job_name)
    {
        // TODO: Mark inputs with red border.
    }
    else
    {
        var fd = new FormData();
        fd.append("ped_file", document.getElementsByName("ped_file")[0].files[0]);
        fd.append("job_name", job_name);
        var xhr = new XMLHttpRequest();
        xhr.upload.addEventListener("progress", uploadProgress, false);
        xhr.addEventListener("load", uploadComplete, false);
        xhr.addEventListener("error", uploadFailed, false);
        xhr.addEventListener("abort", uploadCanceled, false);
        xhr.open("POST", "/api/ped-files");
        xhr.send(fd);
    }
}



document.onreadystatechange = function()
{
    if (document.readyState === "complete")
    {
        document.getElementsByName("ped_upload_form")[0].addEventListener("submit", function(ev)
        {
            ev.preventDefault();
            uploadFile();
        });

        document.getElementById("create_job_button").addEventListener("click", function(ev)
        {
            document.getElementById("upload_overlay").style.display = "block";
        });

        document.getElementById("upload_overlay").addEventListener("click", function(ev)
        {
            hideUploadOverlay();
        });

        document.getElementsByName("ped_upload_form")[0].addEventListener("click", function(ev)
        {
            ev.stopPropagation();
        });

        document.getElementsByName("ped_filename")[0].addEventListener("click", function(ev)
        {
            document.getElementsByName("ped_file")[0].click();
        });

        document.getElementsByName("ped_file")[0].addEventListener("change", function(ev)
        {
            fileSelected();
        });

        document.onkeyup = function(e)
        {
            if (e.keyCode == 27) // ESC
            {
                hideUploadOverlay();
            }
        };

    }
};