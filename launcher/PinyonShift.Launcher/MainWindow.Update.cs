using System.IO;
using System.Net.Http;
using System.Windows;
using PinyonShift.Launcher.Updates;

namespace PinyonShift.Launcher;

// FB-10.7: update from inside the launcher. A newer release on GitHub shows
// "Update to <version>" in the footer; the launcher downloads it beside
// itself, checks GitHub's SHA-256, swaps its files and starts again. The new
// launcher unpacks its source into the same folder, so saves, settings and
// game files stay; the build is then made again from the disc image.
public partial class MainWindow
{
    private AvailableUpdate? _update;
    private bool _updateChecked;

    private string ReleaseJson => Path.Combine(_repositoryRoot!, "config", "release.json");

    // Released launchers only; a checkout updates with git.
    private bool CanUpdate => _repositoryRoot is not null
        && File.Exists(Path.Combine(_repositoryRoot, ".pinyon-source-sha256"));

    private async void ReportUpdateAndCleanUp()
    {
        var arguments = Environment.GetCommandLineArgs();
        var updated = Array.IndexOf(arguments, LauncherUpdate.UpdatedArgument);
        if (updated > 0 && updated + 1 < arguments.Length)
            AppendLog($"Updated from Pinyon Shift {arguments[updated + 1]}. Your saves and settings were kept.");
        try { await LauncherUpdate.CleanUpAsync(LauncherUpdate.InstalledLauncher()); }
        catch (Exception ex) { AppendLog($"Could not remove the previous launcher's files: {ex.Message}"); }
    }

    private async Task CheckForUpdateAsync()
    {
        // A racing wheel build never offers the upstream release: it would replace the wheel source.
        if (_updateChecked || !CanUpdate || !LauncherUpdate.CheckEnabled || _isWheelBuild) return;
        _updateChecked = true;
        try
        {
            using var client = LauncherUpdate.CreateClient(CurrentReleaseVersion());
            _update = await LauncherUpdate.CheckAsync(client, ReleaseJson, CancellationToken.None);
        }
        catch (Exception ex)
        {
            // Offline or rate limited: the launcher works as before.
            AppendLog($"Could not check for updates: {ex.Message}");
            return;
        }
        if (_update is null) return;
        UpdateButtonText.Text = $"Update to {_update.Version}";
        System.Windows.Automation.AutomationProperties.SetName(UpdateButton, UpdateButtonText.Text);
        UpdateButton.ToolTip = $"Pinyon Shift {_update.Version} is available. Your saves and settings are kept.";
        UpdateButton.Visibility = Visibility.Visible;
        AppendLog($"Pinyon Shift {_update.Version} is available: {_update.ReleaseUrl}");
    }

    private string CurrentReleaseVersion() =>
        _repositoryRoot is not null && LauncherUpdate.ReadRelease(ReleaseJson) is { } release ? release.Version : "0.0.0";

    private async void UpdateButton_Click(object sender, RoutedEventArgs e)
    {
        if (_update is not { } update) return;
        if (_busy)
        {
            MessageBox.Show(this, "Close the game and wait for setup to finish, then update.", "Update",
                MessageBoxButton.OK, MessageBoxImage.Information);
            return;
        }
        var size = update.Size > 0 ? $" ({update.Size / (1024.0 * 1024.0):0} MB)" : "";
        var answer = MessageBox.Show(this,
            $"Download Pinyon Shift {update.Version}{size} from GitHub and restart the launcher?\n\n" +
            "Your saves, settings and game files stay where they are. Afterwards, choose your disc image " +
            "to build the new version.\n\nChoose No to read what's new first.",
            "Update Pinyon Shift", MessageBoxButton.YesNoCancel, MessageBoxImage.Question);
        if (answer == MessageBoxResult.No && !string.IsNullOrEmpty(update.ReleaseUrl))
            OpenReleasePage(update.ReleaseUrl);
        if (answer != MessageBoxResult.Yes) return;

        var installed = LauncherUpdate.InstalledLauncher();
        try { LauncherUpdate.EnsureWritable(installed); }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException)
        {
            MessageBox.Show(this,
                $"The launcher's folder cannot be changed ({installed}). Move the launcher to a folder you own, " +
                "such as Documents, or download the new release from GitHub.", "Update",
                MessageBoxButton.OK, MessageBoxImage.Warning);
            OpenReleasePage(update.ReleaseUrl);
            return;
        }

        _busy = true;
        UpdateButton.IsEnabled = false;
        UpdatePrimaryButton();
        ShowPanel(View.Log);
        HeadlineText.Text = $"Updating to {update.Version}";
        SetSubhead("Downloading from GitHub. Your saves and settings stay in place.");
        AppendLog($"Downloading {update.AssetName} {update.Version} from {update.AssetUrl}");
        var staging = LauncherUpdate.StagingRoot(installed);
        try
        {
            _cancellation?.Dispose();
            _cancellation = new CancellationTokenSource();
            using var client = LauncherUpdate.CreateClient(CurrentReleaseVersion());
            var progress = new Progress<double>(fraction =>
                SetProgress((int)(fraction * 100), $"Downloading Pinyon Shift {update.Version}."));
            var download = await LauncherUpdate.DownloadAsync(client, update, staging, progress, _cancellation.Token);
            AppendLog($"SHA-256 matches the release: {update.Sha256}");
            SetProgress(100, "Unpacking the new launcher.");
            var unpacked = await LauncherUpdate.UnpackAsync(download, staging, _cancellation.Token);
            LauncherUpdate.Swap(LauncherUpdate.Plan(unpacked, installed));
            AppendLog($"Installed Pinyon Shift {update.Version}. Restarting the launcher.");
            _sessionLog?.Flush();
            LauncherUpdate.Restart(installed, Environment.GetCommandLineArgs()[1..], CurrentReleaseVersion(),
                ResumeSetupArgument);
            Application.Current.Shutdown();
        }
        catch (Exception ex) when (ex is not OutOfMemoryException)
        {
            try { if (Directory.Exists(staging)) Directory.Delete(staging, recursive: true); }
            catch (Exception cleanup) when (cleanup is IOException or UnauthorizedAccessException) { }
            AppendLog($"Update failed: {ex.Message}");
            _busy = false;
            UpdateButton.IsEnabled = true;
            if (_gameExecutable is not null) SetComplete();
            else
            {
                ResetRoute();
                SetReadyState();
                DetectExistingBuild();
            }
            UpdatePrimaryButton();
            MessageBox.Show(this, $"The update did not finish; this launcher is unchanged.\n\n{ex.Message}",
                "Update failed", MessageBoxButton.OK, MessageBoxImage.Warning);
        }
    }


    // Release pages from the feed open only over HTTPS.
    private static void OpenReleasePage(string url)
    {
        if (Uri.TryCreate(url, UriKind.Absolute, out var uri) && uri.Scheme == Uri.UriSchemeHttps) OpenUrl(url);
    }
}
