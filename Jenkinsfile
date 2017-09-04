#!groovy

node {

    projectName = "nespresso"

    def sanitizedBuildTag = sh script: "echo -n '${env.BUILD_TAG}' | sed -r 's/[^a-zA-Z0-9_.-]//g' | tr '[:upper:]' '[:lower:]'", returnStdout: true

    imageName = "${projectName}-${sanitizedBuildTag}"
    releaseImageName = null

    jeanMichelAbortBuild = false

    try {
        stage('Checkout') {
            checkout scm

            // Jean-Michel Enattendant
            // Pour l'instant Jenkins n'ignore pas les messages de commit chore(release) du à un bug Jenkins, en attendant on fait une chinoiserie.
            // De ce fait on aura 2 builds successifs avec le second en erreur, mais on ne bouclera pas.
            // https://issues.jenkins-ci.org/browse/JENKINS-36836
            // https://issues.jenkins-ci.org/browse/JENKINS-36195
            def lastCommitMessage = sh script: 'git log -1 --pretty=oneline', returnStdout: true
            if (lastCommitMessage =~ /chore\(release\):/) {
                jeanMichelAbortBuild = true
                sh "echo 'Last commit is from Jenkins release, cancel execution'"
                sh "exit 1"
            }
        }

        stage('Build project') {
            docker.build("${imageName}", "--pull -f Dockerfile.build .")

            withCredentials([file(credentialsId: 'iotApiCredentials', variable: 'CREDENTIALS')]) {
                sh "cp $CREDENTIALS Credentials.h && ls -al && cat Credentials.h" // Copy the Credentials.h file into the workspace before building
                sh "docker run --rm -v \$(pwd | sed 's|/root/workspace/|/home/denouche/volumes/jenkins/workspace-tmp/|'):/usr/src/app ${imageName} bash -c 'pwd && mkdir /root/Arduino/libraries/Credentials/ && mv Credentials.h /root/Arduino/libraries/Credentials/ && make clean build && ls -al'"
            }
        }


        if (isRelease()) {
            stage('Release') {
                releaseImageName = "${projectName}-release"
                docker.build(releaseImageName, "--pull -f Dockerfile.release .")

                sshagent (['jenkins-ssh-key']) {
                    sh "docker run --rm -v /home/denouche/volumes/jenkins-agents/.ssh/id_rsa:/root/.ssh/id_rsa -v \$(pwd | sed 's|/root/workspace/|/home/denouche/volumes/jenkins/workspace-tmp/|'):/usr/src/app/ ${releaseImageName} bash -c 'npm i && npm run pre-release'"
                }

                withCredentials([file(credentialsId: 'iotApiCredentials', variable: 'CREDENTIALS')]) {
                    sh "cp $CREDENTIALS Credentials.h && ls -al && cat Credentials.h" // Copy the Credentials.h file into the workspace before building
                    sh "docker run --rm -v \$(pwd | sed 's|/root/workspace/|/home/denouche/volumes/jenkins/workspace-tmp/|'):/usr/src/app ${imageName} bash -c 'pwd && mkdir /root/Arduino/libraries/Credentials/ && mv Credentials.h /root/Arduino/libraries/Credentials/ && make clean build'"
                }

                // Now the release is done if needed, retrieve version number
                pkg = readFile('package.json')
                version = getNPMVersion(pkg)
                application = getNPMApplication(pkg)
                withCredentials([usernameColonPassword(credentialsId: 'iot.leveugle.net', variable: 'BASIC_AUTH')]) {
                    sh """curl -i -X POST -u '${BASIC_AUTH}' \\
                            -H 'Accept:application/json' \\
                            -H 'Content-Type:multipart/form-data' \\
                            -F 'name=${version}' \\
                            -F 'application=${application}' \\
                            -F 'plateform=esp8266' \\
                            -F 'firmware=@'''./dist/${application}.ino.bin''';type=application/octet-stream;filename='''${application}.ino.bin'''' \\
                            'https://iot.leveugle.net/api/versions'"""
                }

                sshagent (['jenkins-ssh-key']) {
                    sh "docker run --rm -v /home/denouche/volumes/jenkins-agents/.ssh/id_rsa:/root/.ssh/id_rsa -v \$(pwd | sed 's|/root/workspace/|/home/denouche/volumes/jenkins/workspace-tmp/|'):/usr/src/app/ ${releaseImageName} bash -c 'npm i && git add ${application}.ino package.json && npm run release'"
                }

                // Push the commit and the git tag only if docker image was successfully pushed
                sshagent (['jenkins-ssh-key']) {
                    sh "git push --follow-tags origin HEAD"
                }
            }
        }
    }
    finally {
        if(jeanMichelAbortBuild) {
            currentBuild.result = 'ABORTED'
        }

        stage('Clean docker') {
            sh "rm Credentials.h || true"
            sh "docker rmi ${releaseImageName} ${imageName} || true"
        }
    }

}


@NonCPS
def getNPMVersion(text) {
    def matcher = text =~ /"version"\s*:\s*"([^"]+)"/
    matcher ? matcher[0][1] : null
}

@NonCPS
def getNPMApplication(text) {
    def matcher = text =~ /"name"\s*:\s*"([^"]+)"/
    matcher ? matcher[0][1] : null
}

@NonCPS
def isRelease() {
    env.BRANCH_NAME == "master" || env.BRANCH_NAME ==~ /^maintenance\/.+/
}
