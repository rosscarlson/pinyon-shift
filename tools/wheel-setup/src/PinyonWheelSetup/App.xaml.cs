using System.Windows;
using PinyonWheelSetup.Theming;

namespace PinyonWheelSetup;

public partial class App : Application
{
    protected override void OnStartup(StartupEventArgs e)
    {
        base.OnStartup(e);
        DispatcherUnhandledException += (_, a) =>
        {
            MessageBox.Show(a.Exception.Message, "Pinyon Wheel Setup", MessageBoxButton.OK, MessageBoxImage.Error);
            a.Handled = true;
        };
        var settings = AppSettings.Load();
        ThemeManager.Apply(settings.Theme);
        var window = new MainWindow(settings, e.Args);
        MainWindow = window;
        window.Show();
    }
}
