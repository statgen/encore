

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



function bindTableRows()
{
    var table_rows = document.getElementById("jobs_table").getElementsByTagName("tr");
    for (var i = 0; i < table_rows.length; ++i)
    {
        if (table_rows[i].hasAttribute("data-id"))
        {
            var r = table_rows[i];
            (function (row_element)
            {
                row_element.addEventListener("click", function (ev)
                {
                    var job_id = row_element.getAttribute("data-id");
                    window.location = "/jobs/" + job_id;
                });
            })(r);
        }
    }
}

function fetchJobs()
{
    var xhr = new XMLHttpRequest();
    xhr.addEventListener("load", function(ev)
    {
        var jobs = JSON.parse(xhr.responseText);
        console.log(jobs);

        var jobs_table = document.getElementById("jobs_table");
        jobs_table.innerHTML = ejs.render(document.getElementById("table_row_tmpl").innerText, {"jobs" : jobs });
        bindTableRows();
    }, false);

    xhr.addEventListener("error", uploadFailed, false);
    xhr.addEventListener("abort", uploadCanceled, false);
    xhr.open("GET", "/api/jobs");
    xhr.send();
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
    try
    {
        var resp = JSON.parse(evt.target.responseText);
        if (!resp)
        {
            alert("A Server Error Occured");
        }
        else if (resp.error)
        {
            alert(resp.error);
        }
        else
        {
            fetchJobs();
            hideUploadOverlay();
        }
    }
    catch(e)
    {
        alert(e);
    }
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
        xhr.open("POST", "/api/jobs");
        xhr.send(fd);
    }
}



document.onreadystatechange = function()
{
    if (document.readyState === "complete")
    {
        fetchJobs();
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

        document.addEventListener("keyup", function(e)
        {
            if (e.keyCode == 27) // ESC
            {
                hideUploadOverlay();
            }
        });

    }
};