#include <errno.h>
#include <gio/gio.h>


#ifndef NOTIFY_H
#define NOTIFY_H

    #define CHECK_NOTIFY(cond, msg) \
        do { \
            if (!(cond)) { \
                char buf[512]; \
                snprintf(buf, sizeof(buf), "[%s:%d] %s: %s: %s", \
                        __FILE__, __LINE__, __func__, msg, strerror(errno)); \
                fprintf(stderr, "%s\n", buf); \
                notify_urgent("Error", buf); \
                exit(EXIT_FAILURE); \
            } \
        } while (0)


    static void _notify(char* title, char* message, char* icon, unsigned int duration, GVariantBuilder* hints){
        GDBusConnection *connection = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
        GVariantBuilder actions;
        GError *error = NULL;

        g_variant_builder_init(&actions, G_VARIANT_TYPE("as"));

        g_dbus_connection_call_sync(
            connection,
            "org.freedesktop.Notifications",
            "/org/freedesktop/Notifications",
            "org.freedesktop.Notifications",
            "Notify",
            // 0 means New notification, no replaces_id | app icon set to ""
            g_variant_new("(susssasa{sv}i)",
                "MyApp", 0, icon, title, (message ? message : ""),
                &actions, hints, duration * 1000
            ),
            NULL,
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            NULL,
            &error
        );

        if (error)
            exit(EXIT_FAILURE);

        g_object_unref(connection);
    }


    static void notify(char* title, char* message, unsigned int duration){
        GVariantBuilder hints;
        g_variant_builder_init (&hints, G_VARIANT_TYPE("a{sv}"));
        _notify(title, message, "", duration, &hints);
    }


    static void notify_urgent(char* title, char* message){
        // Set urgency | https://github.com/PipeWire/wireplumber/blob/c2b96ebb395a8e9f97e728cff461d480446b94cb/modules/module-notifications-api.c#L52-L53
        GVariantBuilder hints;
        g_variant_builder_init (&hints, G_VARIANT_TYPE("a{sv}"));
        g_variant_builder_add (&hints, "{sv}", "urgency", g_variant_new_byte (2));
        _notify(title, message, "", 0, &hints);
    }

#endif
