// Generated on 2014-05-29 using generator-angular 0.7.1
'use strict';

// # Globbing
// for performance reasons we're only matching one level down:
// 'test/spec/{,*/}*.js'
// use this if you want to recursively match all subfolders:
// 'test/spec/**/*.js'

module.exports = function (grunt) {

    // Load grunt tasks automatically
    require('load-grunt-tasks')(grunt);


    var cloudHost = 'cloud-test.hdw.mx';  // 'cloud-local' // For local vagrant

    // Time how long tasks take. Can help when optimizing build times
    require('time-grunt')(grunt);

    grunt.loadNpmTasks('grunt-protractor-webdriver');
    grunt.loadNpmTasks('grunt-protractor-runner');
    grunt.loadNpmTasks('grunt-wiredep');
    grunt.loadNpmTasks('grunt-backstop');
    grunt.loadNpmTasks('grunt-connect-rewrite');

    // Define the configuration for all the tasks
    grunt.initConfig({

        // Project settings
        yeoman: {
            // configurable paths
            app: require('./bower.json').appPath || 'app',
            config: grunt.file.exists('config.json') && grunt.file.readJSON('config.json'),
            dist: 'dist'
        },

        // Automatically inject Bower components into the app
        wiredep: {
            target: {
                src: [
                    '<%= yeoman.app %>/*.html',
                    '<%= yeoman.app %>/index.xsl'
                ],
                ignorePath: '<%= yeoman.app %>/'
            }
        },
        // Watches files for changes and runs tasks based on the changed files
        watch: {
            js: {
                files: ['<%= yeoman.app %>/scripts/**','<%= yeoman.app %>/components/**'],
                tasks: ['newer:jshint:all'],
                options: {
                    livereload: true
                }
            },
            jsTest: {
                files: ['test/spec/{,*/}*.js'],
                tasks: ['newer:jshint:test', 'karma']
            },
            compass: {
                files: ['<%= yeoman.app %>/styles/{,*/}*.{scss,sass}', 'customizations/<%= yeoman.config.customization %>/front_end/{,*/}*.{scss,sass}'],
                tasks: ['compass:server', 'autoprefixer']
            },
            gruntfile: {
                files: ['Gruntfile.js']
            },
            livereload: {
                options: {
                    livereload: '<%= connect.options.livereload %>'
                },
                files: [
                    '<%= yeoman.app %>/**.html',
                    '.tmp/styles/{,*/}*.css',
                    '<%= yeoman.app %>/images/{,*/}*.{png,jpg,jpeg,gif,webp,svg}'
                ]
            }
        },


        // The actual grunt server settings
        connect: {
            options: {
                port: 9000,
                // Change this to '0.0.0.0' to access the server from outside.
                hostname: '0.0.0.0',
                livereload: 35729
            },
            rules: [
                {from: '^((?!\\.).)*$', to: '/index.html'},
                {from: '^/static/(.*)$', to: '/$1'}
            ],
            proxies: [
                // cloud-demo.hdw.mx
                {context: '/api/',    host: cloudHost, port: 80},
                {context: '/notifications/',    host: cloudHost, port: 80}/**/
            ],
            livereload: {
                options: {
                    middleware: function (connect, options) {
                        var serveStatic = require('serve-static');
                        if (!Array.isArray(options.base)) {
                              options.base = [options.base];
                        }

                        // Setup the proxy
                        var middlewares = [
                            require('grunt-connect-proxy/lib/utils').proxyRequest,
                            require('grunt-connect-rewrite/lib/utils').rewriteRequest
                            ];

                        // Serve static files.
                        options.base.forEach(function (base) {
                            middlewares.push(serveStatic(base));
                        });


                        // Make directory browse-able.
                        //var directory = options.directory || options.base[options.base.length - 1];
                        //middlewares.push(connect.directory(directory));

                        return middlewares;
                    },
                    open: true,
                    base: [
                        '.tmp',
                        'test',
                        '<%= yeoman.app %>'
                    ]
                }
            },
            test: {
                options: {
                    middleware: function (connect, options) {

                        var serveStatic = require('serve-static');
                        if (!Array.isArray(options.base)) {
                            options.base = [options.base];
                        }

                        // Setup the proxy
                        var middlewares = [require('grunt-connect-proxy/lib/utils').proxyRequest];

                        // Serve static files.
                        options.base.forEach(function (base) {
                            middlewares.push(serveStatic(base));
                        });

                        // Make directory browse-able.
                        // var directory = options.directory || options.base[options.base.length - 1];
                        // middlewares.push(connect.directory(directory));

                        return middlewares;
                    },
                    port: 9000,

                    base: [
                        '.tmp',
                        'test',
                        '<%= yeoman.app %>'
                    ]
                }
            },
            dist: {
                options: {
                    middleware: function (connect, options) {
                        if (!Array.isArray(options.base)) {
                            options.base = [options.base];
                        }

                        // Setup the proxy
                        var middlewares = [require('grunt-connect-proxy/lib/utils').proxyRequest];

                        // Serve static files.
                        options.base.forEach(function (base) {
                            middlewares.push(connect.static(base));
                        });



                        var rewriteRulesSnippet = require('grunt-connect-rewrite/lib/utils').rewriteRequest;
                        middlewares.push(rewriteRulesSnippet);

                        // Make directory browse-able.
                        var directory = options.directory || options.base[options.base.length - 1];
                        middlewares.push(connect.directory(directory));

                        return middlewares;
                    },
                    open: true,
                    base: '<%= yeoman.dist %>'
                }
            }
        },

        // Make sure code styles are up to par and there are no obvious mistakes
        jshint: {
            options: {
                jshintrc: '.jshintrc',
                reporter: require('jshint-stylish')
            },
            all: [
                'Gruntfile.js',
                '<%= yeoman.app %>/scripts/{,*/}*.js'
            ],
            test: {
                options: {
                    jshintrc: 'test/.jshintrc'
                },
                src: ['test/spec/{,*/}*.js']
            }
        },

        // Empties folders to start fresh
        clean: {
            compiled:{
                files: [
                    {
                        dot: true,
                        src: [
                            '<%= yeoman.dist %>/static'
                        ]
                    }
                ]
            },
            dist: {
                files: [
                    {
                        dot: true,
                        src: [
                            '.tmp',
                            '<%= yeoman.dist %>/*',
                            '!<%= yeoman.dist %>/.git*'
                        ]
                    }
                ]
            },
            server: {
                files: [
                    {
                        dot: true,
                        src: [
                            '.tmp',
                            '<%= yeoman.app %>/styles/custom'
                        ]
                    }
                ]
            },


            publish:{
                files:[
                    {
                        dot: true,
                        src: '<%= yeoman.dist %>/../../cloud/static/*'
                    }
                ],
                options: {
                    force: true
                }
            }
        },

        // Add vendor prefixed styles
        autoprefixer: {
            options: {
                browsers: ['last 1 version']
            },
            dist: {
                files: [
                    {
                        expand: true,
                        cwd: '.tmp/styles/',
                        src: '{,*/}*.css',
                        dest: '.tmp/styles/'
                    }
                ]
            }
        },

        // Automatically inject Bower components into the app
       
        // Compiles Sass to CSS and generates necessary files if requested
        compass: {
            options: {
                sassDir: '<%= yeoman.app %>/styles',
                cssDir: '.tmp/styles',
                generatedImagesDir: '.tmp/images/generated',
                imagesDir: '<%= yeoman.app %>/images',
                javascriptsDir: '<%= yeoman.app %>/scripts',
                fontsDir: '<%= yeoman.app %>/fonts',
                importPath: '<%= yeoman.app %>/bower_components',
                httpImagesPath: '/images',
                httpGeneratedImagesPath: '/images/generated',
                httpFontsPath: '/fonts',
                relativeAssets: false,
                assetCacheBuster: false,
                raw: 'Sass::Script::Number.precision = 10\n'
            },
            dist: {
                options: {
                    generatedImagesDir: '<%= yeoman.dist %>/images/generated'
                }
            },
            server: {
                options: {
                    debugInfo: true
                }
            }
        },

        // Renames files for browser caching purposes
        rev: {
            dist: {
                files: {
                    src: [
                        '<%= yeoman.dist %>/static/scripts/{,*/}*.js',
                        '<%= yeoman.dist %>/static/styles/{,*/}*.css',

                        '<%= yeoman.dist %>/scripts/{,*/}*.js',
                        '!<%= yeoman.dist %>/scripts/{,*/}language.js',
                        '!<%= yeoman.dist %>/scripts/{,*/}downloads.js',
                        '<%= yeoman.dist %>/styles/{,*/}*.css'//,
                        //'<%= yeoman.dist %>/images/{,*/}*.{png,jpg,jpeg,gif,webp,svg}',
                        //'<%= yeoman.dist %>/fonts/*'
                    ]
                }
            }
        },

        // Reads HTML for usemin blocks to enable smart builds that automatically
        // concat, minify and revision files. Creates configurations in memory so
        // additional tasks can operate on them
        useminPrepare: {
            html: ['<%= yeoman.app %>/*.html', '<%= yeoman.app %>/api.xsl'],
            options: {
                dest: '<%= yeoman.dist %>'
            }
        },

        // Performs rewrites based on rev and the useminPrepare configuration
        usemin: {
            html: ['<%= yeoman.dist %>/{*.html,*.xsl}'],
            css: ['<%= yeoman.dist %>/styles/{,*/}*.css'],
            options: {
                assetsDirs: [
                    '<%= yeoman.dist %>',
                    '<%= yeoman.dist %>/fonts',

                    '<%= yeoman.app %>',
                    '<%= yeoman.app %>/fonts'
                ]
            }
        },

        // The following *-min tasks produce minified files in the dist folder
        imagemin: {
            dist: {
                files: [
                    {
                        expand: true,
                        cwd: '<%= yeoman.app %>/images',
                        src: '{,*/}*.{png,jpg,jpeg}',
                        dest: '<%= yeoman.dist %>/images'
                    }
                ]
            }
        },
        svgmin: {
            dist: {
                files: [
                    {
                        expand: true,
                        cwd: '<%= yeoman.app %>/images',
                        src: '{,*/}*.svg',
                        dest: '<%= yeoman.dist %>/images'
                    }
                ]
            }
        },
        htmlmin: {
            dist: {
                options: {
                    collapseWhitespace: true,
                    collapseBooleanAttributes: true,
                    removeCommentsFromCDATA: true,
                    removeOptionalTags: true
                },
                files: [
                    {
                        expand: true,
                        cwd: '<%= yeoman.dist %>',
                        src: ['*.html', 'views/{,*/}*.html'],
                        dest: '<%= yeoman.dist %>'
                    }
                ]
            }
        },

        // Allow the use of non-minsafe AngularJS files. Automatically makes it
        // minsafe compatible so Uglify does not destroy the ng references
        ngmin: {
            dist: {
                files: [
                    {
                        expand: true,
                        cwd: '.tmp/concat/scripts',
                        src: '*.js',
                        dest: '.tmp/concat/scripts'
                    }
                ]
            }
        },

        // Copies remaining files to places other tasks can use
        copy: {
            custom: {
                expand: true,
                cwd: '../customizations/<%= yeoman.config.customization %>/front_end',
                dest: '.tmp/',
                src: '**'
            },
            custom_css: {
                expand: true,
                cwd: '../customizations/<%= yeoman.config.customization %>/front_end/styles/',
                dest: '<%= yeoman.app %>/styles/custom/',
                src: '**'
            },
            compiled:{
                expand: true,
                cwd: '<%= yeoman.dist %>/static',
                dest: '<%= yeoman.dist %>',
                src: ['**']
            },
            dist: {
                files: [
                    {
                        expand: true,
                        dot: true,
                        cwd: '<%= yeoman.app %>',
                        dest: '<%= yeoman.dist %>',
                        src: [
                            '*.{ico,png,txt}',
                            '.htaccess',
                            '*.html',
                            'apple-app-site-association',
                            'scripts/{,*/}*.json',
                            'scripts/language.js',
                            'scripts/downloads.js',
                            'views/{,*/}*.html',
                            'customization/*',
                            //'bower_components/**/*',
                            //'bower_components/videogular-themes-default/videogular.css',
                            'images/**',
                            'fonts/**',
                            'components/**'
                        ]
                    },
                    {
                        expand: true,
                        cwd: '../customizations/<%= yeoman.config.customization %>/front_end',
                        dest: '<%= yeoman.dist %>/',
                        src: '**'
                    },
                    {
                        expand: true,
                        cwd: '.tmp/images',
                        dest: '<%= yeoman.dist %>/images',
                        src: ['generated/*']
                    },
                    {
                        expand: true,
                        cwd: '<%= yeoman.app %>',
                        flatten: true,
                        dest: '<%= yeoman.dist %>/fonts',
                        src: ['bower_components/bootstrap-sass/assets/fonts/*']
                    }
                ]
            },
            styles: {
                expand: true,
                cwd: '<%= yeoman.app %>/styles',
                dest: '.tmp/styles/',
                src: '{,*/}*.css'
            },
            publish:{
                expand: true,
                nonull:true,
                cwd: '<%= yeoman.dist %>',
                dest: '../cloud/static/',
                src: '**'
            }
        },

        // Run some tasks in parallel to speed up the build process
        concurrent: {
            server: [
                'compass:server'
            ],
            test: [
                'compass'
            ],
            dist: [
                'compass:dist',
                //'imagemin',
                'svgmin'
            ]
        },

        // Test settings
        karma: {
            unit: {
                configFile: 'karma.conf.js',
                singleRun: true
            }
        },
        protractor: {
            options: {
                configFile: 'protractor-conf.js', // Default config file
                keepAlive: true, // If false, the grunt process stops when the test fails.
                noColor: false, // If true, protractor will not use colors in its output.
                args: {
                    // Arguments passed to the command
                }
            },
            all: {   // Grunt requires at least one target to run so you can simply put 'all: {}' here too.
                options: {
                    args: {specs: ['test/e2e/*/*spec.js']} // Target-specific arguments
                }
            },
            restore_all_pass: {
                options: {
                    args: {specs: ['test/e2e/_restore-all-passwords/*spec.js']} // Target-specific arguments
                }
            },
            account: {
                options: {
                    args: {specs: ['test/e2e/account/*spec.js']} // Target-specific arguments
                }
            },
            login: {
                options: {
                    args: {specs: ['test/e2e/login/*spec.js']} // Target-specific arguments
                }
            },
            register: {
                options: {
                    args: {specs: ['test/e2e/register/*spec.js']} // Target-specific arguments
                }
            },
            restore_pass: {
                options: {
                    args: {specs: ['test/e2e/restore_pass/*spec.js']} // Target-specific arguments
                }
            },
            system_page: {
                options: {
                    args: {specs: ['test/e2e/system_page/*spec.js']} // Target-specific arguments
                }
            },
            systems: {
                options: {
                    args: {specs: ['test/e2e/systems/*spec.js']} // Target-specific arguments
                }
            }
        },
        protractor_webdriver: {
            options:{
                keepAlive : true,
                command: 'webdriver-manager start -Djava.security.egd=file:///dev/urandom'
            },
            default:{

            }
        },

        // Screenshot-based testing
        backstop: {
            setup: {
                options : {
                    backstop_path: './node_modules/backstopjs',
                    test_path: './test/visual',
                    setup: false,
                    configure: true
                }
            },
            test: {
                options : {
                    backstop_path: './node_modules/backstopjs',
                    test_path: './test/visual',
                    create_references: false,
                    run_tests: true
                    }
                },
            reference: {
                options : {
                    backstop_path: './node_modules/backstopjs',
                    test_path: './test/visual',
                    create_references: true,
                    run_tests: false
                }
            }
        },

        shell:{
            updateTest:{
                command: 'cd ../build_scripts; ./build.sh; cd ../../nx_cloud_deploy/cloud_portal; ./make.sh publish cloud-test'
            },
            merge:{
                command: 'hg pull -u; python ../../devtools/util/merge_dev.py -t prod_3.0.0; hg push;'
            },
            version: {
                command: 'hg parent > dist/version.txt'
            },
            print_version: {
                command: 'hg parent'
            }

        }
    });


    grunt.registerTask('setbranding', function (branding) {
        var config = {customization: branding};
        grunt.file.write('config.json', JSON.stringify(config, null, 2) + '\n');
        grunt.task.run(['build']);
    });


    grunt.registerTask('serve', function (target) {
        if (target === 'dist') {
            return grunt.task.run([
                'build',
                'configureProxies:server',
                //'connect:livereload',
                'connect:dist:keepalive'
            ]);
        }

        grunt.task.run([
            'clean:server',
            'wiredep',
            'copy:custom',
            'copy:custom_css',
            'concurrent:server',
            'autoprefixer',
            'configureRewriteRules',
            'configureProxies:server',
            'connect:livereload',
            'watch'
        ]);
    });

    grunt.registerTask('server', function () {
        grunt.log.warn('The `server` task has been deprecated. Use `grunt serve` to start a server.');
        grunt.task.run(['serve']);
    });

    /* Instead of grunt test use following command to run suits separately
    grunt test-restore-all-pass; grunt test-account; grunt test-login; grunt test-register; grunt test-restore-pass; grunt test-syspage; grunt test-systems
    */
    grunt.registerTask('test', [
        'clean:server',
        'copy:custom_css',
        'concurrent:test',
        'configureProxies:server',
        'autoprefixer',
        'connect:test',
        'protractor_webdriver',
        'shell:print_version',
        'protractor:all'
        //'newer:jshint'
        //'karma'
    ]);
    grunt.registerTask('test-restore-all-pass', [
        'clean:server',
        'copy:custom_css',
        'concurrent:test',
        'configureProxies:server',
        'autoprefixer',
        'connect:test',
        'protractor_webdriver',
        'shell:print_version',
        'protractor:restore_all_pass'
    ]);
    grunt.registerTask('test-account', [
        'clean:server',
        'copy:custom_css',
        'concurrent:test',
        'configureProxies:server',
        'autoprefixer',
        'connect:test',
        'protractor_webdriver',
        'shell:print_version',
        'protractor:account'
    ]);
    grunt.registerTask('test-login', [
        'clean:server',
        'copy:custom_css',
        'concurrent:test',
        'configureProxies:server',
        'autoprefixer',
        'connect:test',
        'protractor_webdriver',
        'shell:print_version',
        'protractor:login'
    ]);
    grunt.registerTask('test-register', [
        'clean:server',
        'copy:custom_css',
        'concurrent:test',
        'configureProxies:server',
        'autoprefixer',
        'connect:test',
        'protractor_webdriver',
        'shell:print_version',
        'protractor:register'
    ]);
    grunt.registerTask('test-restore-pass', [
        'clean:server',
        'copy:custom_css',
        'concurrent:test',
        'configureProxies:server',
        'autoprefixer',
        'connect:test',
        'protractor_webdriver',
        'shell:print_version',
        'protractor:restore_pass'
    ]);
    grunt.registerTask('test-syspage', [
        'clean:server',
        'copy:custom_css',
        'concurrent:test',
        'configureProxies:server',
        'autoprefixer',
        'connect:test',
        'protractor_webdriver',
        'shell:print_version',
        'protractor:system_page'
    ]);
    grunt.registerTask('test-systems', [
        'clean:server',
        'copy:custom_css',
        'concurrent:test',
        'configureProxies:server',
        'autoprefixer',
        'connect:test',
        'protractor_webdriver',
        'shell:print_version',
        'protractor:systems'
    ]);

    // Perform backstop:test to compare screenshots for locations specified at backstop.json with latest ones
    grunt.registerTask('testshots', [
        'clean:server',
        'copy:custom_css',
        'concurrent:test',
        'configureProxies:server',
        'autoprefixer',
        'connect:test',
        'shell:print_version',
        'backstop:test'
    ]);
    // Perform backstop:reference to create screenshots for locations specified at backstop.json
    grunt.registerTask('refshots', [
        'clean:server',
        'copy:custom_css',
        'concurrent:test',
        'configureProxies:server',
        'autoprefixer',
        'connect:test',
        'shell:print_version',
        'backstop:reference'
    ]);

    grunt.registerTask('code', [
        'newer:jshint'
    ]);

    grunt.registerTask('build', [
        'clean:dist',
        'wiredep',
        'useminPrepare',
        'copy:custom_css',
        'concurrent:dist',
        'autoprefixer',
        'concat',
        'ngmin',
        'copy:custom',
        'copy:dist',
        'cssmin',
        'uglify',
        'rev',
        'usemin',
        'copy:compiled',
        'clean:compiled',
        'htmlmin',
        'shell:version'
    ]);

    grunt.registerTask('default', [
        'newer:jshint',
        'test',
        'build'
    ]);


    grunt.registerTask('publish', [
        'build',
        'clean:publish',
        'copy:publish'
    ]);

    grunt.registerTask('pub', [
        'publish'
    ]);


    grunt.registerTask('update', [
        'shell:updateTest'
    ]);


    grunt.registerTask('merge', [
        'shell:merge'
    ]);

};
